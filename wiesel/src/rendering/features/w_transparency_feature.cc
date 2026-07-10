
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_transparency_feature.h"
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_instance_batcher.h"
#include "rendering/w_mesh_render_utils.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "scene/w_scene.h"

namespace wiesel {

TransparencyFeature::TransparencyFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Pipeline: alpha blend on, depth test on, depth write off, no culling
  auto vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/geometry_shader.vert"});
  auto frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/transparency_shader.frag"});
  pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeNone, false, true, true, false});
  pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                           Vertex3D::GetAttributeDescriptions());
  pipeline_->AddColorAttachment(renderer_->GetSwapChainImageFormat());
  pipeline_->SetDepthAttachment(renderer_->FindDepthFormat());
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("GeometryMesh"));
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Global"));
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Bone"));
  pipeline_->AddShader(vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();

  // IBL variant: same but with irradiance/prefilter/brdfLUT samplers
  auto ibl_frag =
      renderer_->CreateShader({ShaderTypeFragment,
                               ShaderLangGLSL,
                               "main",
                               ShaderSourceSource,
                               "engine://shaders/transparency_shader.frag",
                               {"USE_IBL"}});
  ibl_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeNone, false, true, true, false});
  ibl_pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                               Vertex3D::GetAttributeDescriptions());
  ibl_pipeline_->AddColorAttachment(renderer_->GetSwapChainImageFormat());
  ibl_pipeline_->SetDepthAttachment(renderer_->FindDepthFormat());
  ibl_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("GeometryMesh"));
  ibl_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Global"));
  ibl_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Bone"));
  ibl_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("IBL"));
  ibl_pipeline_->AddShader(vert);
  ibl_pipeline_->AddShader(ibl_frag);
  ibl_pipeline_->Bake();
}

void TransparencyFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("TransparencyFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  pool.SetTexture(
      "transparency.color",
      renderer.CreateAttachmentTexture(
          {rw, rh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));

  auto output_desc = std::make_shared<DescriptorSet>();
  output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("transparency.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  output_desc->Bake();
  pool.SetDescriptor("transparency.output", output_desc);
}

void TransparencyFeature::AddPasses(RenderGraph& graph,
                                    RenderResourceRegistry& registry,
                                    RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("TransparencyFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  MultiScene& scenes = ctx.scenes;

  RGResource transparency_out = graph.ImportTexture(
      "TransparencyOut", pool->GetTexture("transparency.color"));
  RGResource transparency_depth = graph.ImportTexture(
      "TransparencyDepth", pool->GetTexture("geometry.depth_stencil"));

  RGResource lighting_out = registry.Get("LightingOut");

  bool use_ibl =
      pool->HasDescriptor("ibl.descriptor") && renderer_->options().ibl_enabled;
  auto active_pipeline = use_ibl ? ibl_pipeline_ : pipeline_;
  auto ibl_desc = use_ibl ? pool->GetDescriptor("ibl.descriptor") : nullptr;

  uint32_t pass = graph.AddPass(
      "Transparency",
      [active_pipeline, &scenes, renderer, ibl_desc](VkCommandBuffer cmd) {
        active_pipeline->Bind(cmd);

        auto global_desc =
            renderer->GetCameraData()->resource_pool->GetDescriptor(
                "GlobalDescriptor");
        auto bone_desc = renderer->GetIdentityBoneDescriptor();

        MeshInstanceBatcher batcher(renderer.get());
        scenes.ForEach<MeshRendererComponent, TransformComponent>(
            [&](Scene& scene, uint8_t scene_idx, entt::entity entity) {
              auto& mr = scene.GetComponent<MeshRendererComponent>(entity);
              if (!mr.enable_rendering || !mr.model_handle.IsValid()) {
                return;
              }
              Renderer::MeshDrawPrep prep;
              if (!renderer->PrepareMesh(mr, prep)) {
                return;
              }
              if (!prep.mesh->has_transparency) {
                return;
              }
              auto& transform = scene.GetComponent<TransformComponent>(entity);
              MatricesUniformData data =
                  BuildInstanceData(mr, transform, entity, scene_idx);
              batcher.Add(prep.mesh, prep.material, prep.geometry_descriptor,
                          data);
            });
        batcher.Flush(cmd, active_pipeline.get(), global_desc, bone_desc,
                      ibl_desc);

        scenes.ForEach<SkinnedMeshRendererComponent, TransformComponent>(
            [&](Scene& scene, entt::entity entity) {
              auto& mr =
                  scene.GetComponent<SkinnedMeshRendererComponent>(entity);
              if (!mr.enable_rendering || !mr.model_handle.IsValid()) {
                return;
              }
              const TransformComponent* draw_transform =
                  &scene.GetComponent<TransformComponent>(entity);
              const SkeletalAnimRuntime* skel = nullptr;
              ResolveSkeletonRoot(scene, mr, draw_transform, skel);
              renderer->DrawSkinnedMeshRenderer(mr, *draw_transform, skel,
                                                false, true, entity, ibl_desc);
            });
      });

  graph.PassWritesColor(pass, transparency_out);
  graph.PassWritesDepthLoad(pass, transparency_depth);
  if (lighting_out.IsValid()) {
    graph.PassReadsTexture(pass, lighting_out);
  }
  graph.SetPassViewport(pass, ctx.viewport_size);
  graph.SetPassClearColor(pass, {0, 0, 0, 0});

  registry.Register("TransparencyOut", transparency_out);
}

bool TransparencyFeature::IsEnabled(const RenderContext& ctx) const {
  // Disabled when MSAA is active (depth buffer format mismatch)
  return ctx.renderer.options().msaa_mode == SamplingMode::DISABLED;
}

}  // namespace wiesel
