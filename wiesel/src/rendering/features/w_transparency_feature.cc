
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
#include "asset/w_asset_manager.h"
#include "rendering/w_mesh.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace Wiesel {

TransparencyFeature::TransparencyFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Render pass: 1 color + depth stencil (read-only)
  render_pass_ = std::make_shared<RenderPass>(PassType::ForwardTransparency,
                                              "Transparency RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->AttachOutput({.type = AttachmentTextureType::DepthStencil,
                              .format = renderer_->FindDepthFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

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
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("GeometryMesh"));
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Global"));
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Bone"));
  pipeline_->AddShader(vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();
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

  // Framebuffer: transparency color + geometry depth stencil (reused, read-only)
  std::array<AttachmentTexture*, 2> attachments{
      pool.GetTexture("transparency.color").get(),
      pool.GetTexture("geometry.depth_stencil").get()};
  pool.SetFramebuffer("transparency", render_pass_->CreateFramebuffer(
                                          0, attachments, {rw, rh}));

  // Output descriptor for composite
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
  auto* scene = &ctx.scene;
  auto pipeline = pipeline_;

  RGResource transparency_out = graph.ImportTexture(
      "TransparencyOut", pool->GetTexture("transparency.color"));

  // Depend on LightingOut for ordering (ensures geometry + lighting run first)
  auto lighting_out = registry.Get("LightingOut");

  uint32_t pass = graph.AddPass(
      "Transparency", render_pass_,
      [pipeline, scene, renderer](VkCommandBuffer) {
        pipeline->Bind(PipelineBindPointGraphics);
        const FrustumPlanes& frustum = renderer->GetCameraData()->planes;
        auto& assets = Engine::asset_manager();
        for (const auto& entity :
             scene->GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
          auto& model = scene->GetComponent<ModelComponent>(entity);
          if (!model.enable_rendering) {
            continue;
          }
          auto& transform = scene->GetComponent<TransformComponent>(entity);
          const auto& model_data = assets.GetOrLoad<Model>(model.model_handle);
          if (model_data && model_data->bounds.Valid()) {
            AABB world_bounds =
                model_data->bounds.Transformed(transform.GetTransformMatrix());
            if (frustum.IsBoxOutside(world_bounds.min, world_bounds.max)) {
              continue;
            }
          }
          renderer->DrawModelTransparent(model, transform, entity);
        }
      });

  graph.PassWritesColor(pass, transparency_out);
  if (lighting_out.IsValid()) {
    graph.PassReadsTexture(pass, lighting_out);
  }
  graph.SetPassFramebuffer(pass, pool->GetFramebuffer("transparency"));
  graph.SetPassViewport(pass, ctx.viewport_size);
  graph.SetPassClearColor(pass, {0, 0, 0, 0});

  registry.Register("TransparencyOut", transparency_out);
}

bool TransparencyFeature::IsEnabled(const RenderContext& ctx) const {
  // Disabled when MSAA is active (depth buffer format mismatch)
  return ctx.renderer.options().msaa_mode == SamplingMode::DISABLED;
}

}  // namespace Wiesel
