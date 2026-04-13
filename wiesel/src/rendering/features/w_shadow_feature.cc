
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_shadow_feature.h"
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_render_utils.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_scene.h"

namespace wiesel {

ShadowFeature::ShadowFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  render_pass_ =
      std::make_shared<RenderPass>(PassType::Shadow, "Shadow RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::DepthStencil,
                              .format = renderer_->FindDepthFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  auto vert = renderer_->CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                                       ShaderSourceSource,
                                       "engine://shaders/shadow_shader.vert"});
  auto frag = renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL,
                                       "main", ShaderSourceSource,
                                       "engine://shaders/shadow_shader.frag"});
  pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, false, true, true});
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                           Vertex3D::GetAttributeDescriptions());
  push_constant_ = std::make_shared<ShadowPipelinePushConstant>();
  pipeline_->AddPushConstant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT);
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("GeometryMesh"));
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("GlobalShadow"));
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Bone"));
  pipeline_->AddShader(vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();

  pipeline_no_cull_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeNone, false, false, true, true});
  pipeline_no_cull_->SetRenderPass(render_pass_);
  pipeline_no_cull_->SetVertexData(Vertex3D::GetBindingDescription(),
                                   Vertex3D::GetAttributeDescriptions());
  pipeline_no_cull_->AddPushConstant(push_constant_,
                                     VK_SHADER_STAGE_VERTEX_BIT);
  pipeline_no_cull_->AddInputLayout(
      renderer_->GetDescriptorLayout("GeometryMesh"));
  pipeline_no_cull_->AddInputLayout(
      renderer_->GetDescriptorLayout("GlobalShadow"));
  pipeline_no_cull_->AddInputLayout(renderer_->GetDescriptorLayout("Bone"));
  pipeline_no_cull_->AddShader(vert);
  pipeline_no_cull_->AddShader(frag);
  pipeline_no_cull_->Bake();
}

void ShadowFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("ShadowFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  auto& camera = ctx.camera;

  uint32_t shadow_dim =
      static_cast<uint32_t>(renderer.options().shadow_map_resolution.Get());

  // Layered depth texture for all cascades
  pool.SetTexture(
      "shadow.depth_stencil",
      renderer.CreateAttachmentTexture(
          {shadow_dim, shadow_dim, AttachmentTextureType::DepthStencil, 1,
           renderer.FindDepthFormat(), SamplingMode::DISABLED, true,
           WIESEL_SHADOW_CASCADE_COUNT}));

  auto shadow_depth = pool.GetTexture("shadow.depth_stencil");

  // Array view spanning all cascade layers (used by the global descriptor / lighting shader)
  pool.SetImageView(
      "ShadowDepthViewArray",
      renderer.CreateImageView(shadow_depth, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0,
                               WIESEL_SHADOW_CASCADE_COUNT));

  // Per-cascade image views and framebuffers
  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    auto cascade_view =
        renderer.CreateImageView(shadow_depth, VK_IMAGE_VIEW_TYPE_2D, i);
    std::array<ImageView*, 1> views = {
        cascade_view.get(),
    };
    pool.SetImageView("ShadowDepthView" + std::to_string(i), cascade_view);
    pool.SetFramebuffer(
        "shadow.fb." + std::to_string(i),
        render_pass_->CreateFramebuffer(views, {shadow_dim, shadow_dim}));
  }

  // Shadow global descriptor
  pool.SetDescriptor("ShadowGlobalDescriptor",
                     renderer.CreateShadowGlobalDescriptors(camera));
}

void ShadowFeature::AddPasses(RenderGraph& graph,
                              RenderResourceRegistry& registry,
                              RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("ShadowFeature::AddPasses");
  auto& pool = ctx.resources;

  // Import the layered shadow depth texture
  RGResource shadow_depth;
  auto shadow_tex = pool.GetTexture("shadow.depth_stencil");
  if (shadow_tex) {
    shadow_depth = graph.ImportTexture("ShadowDepth", shadow_tex);
  }

  // Capture stable pointers for deferred lambda execution.
  MultiScene& scenes = ctx.scenes;
  auto renderer = renderer_;
  auto pipeline = pipeline_;
  auto pipeline_no_cull = pipeline_no_cull_;
  auto push_constant = push_constant_;
  bool does_shadow = ctx.camera.does_shadow_pass;

  // Helper lambdas for drawing shadow casters with frustum culling
  auto draw_static_meshes = [&scenes, renderer](bool double_sided_pass) {
    const FrustumPlanes& frustum = renderer->GetCameraData()->planes;
    scenes.ForEach<MeshRendererComponent, TransformComponent>(
        [&](Scene& scene, entt::entity entity) {
          auto& mr = scene.GetComponent<MeshRendererComponent>(entity);
          if (!mr.receive_shadows || !mr.enable_rendering ||
              !mr.model_handle.IsValid()) {
            return;
          }
          if (IsMeshDoubleSided(mr.model_handle, mr.mesh_index) !=
              double_sided_pass) {
            return;
          }
          auto& transform = scene.GetComponent<TransformComponent>(entity);
          if (FrustumCullMesh(frustum, mr.model_handle, mr.mesh_index,
                              transform.GetTransformMatrix())) {
            return;
          }
          renderer->DrawMeshRenderer(mr, transform, true);
        });
  };

  auto draw_skinned_meshes = [&scenes, renderer](bool double_sided_pass) {
    const FrustumPlanes& frustum = renderer->GetCameraData()->planes;
    scenes.ForEach<SkinnedMeshRendererComponent, TransformComponent>(
        [&](Scene& scene, entt::entity entity) {
          auto& mr = scene.GetComponent<SkinnedMeshRendererComponent>(entity);
          if (!mr.receive_shadows || !mr.enable_rendering ||
              !mr.model_handle.IsValid()) {
            return;
          }
          if (IsMeshDoubleSided(mr.model_handle, mr.mesh_index) !=
              double_sided_pass) {
            return;
          }
          const TransformComponent* draw_transform =
              &scene.GetComponent<TransformComponent>(entity);
          const SkeletalAnimRuntime* skel = nullptr;
          if (!ResolveSkeletonRoot(scene, mr, draw_transform, skel)) {
            return;
          }
          if (FrustumCullSkinned(frustum, skel,
                                 draw_transform->GetTransformMatrix())) {
            return;
          }
          renderer->DrawSkinnedMeshRenderer(mr, *draw_transform, skel, true);
        });
  };

  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    uint32_t shadow = graph.AddPass(
        "Shadow " + std::to_string(i), render_pass_,
        [pipeline, pipeline_no_cull, push_constant, renderer, does_shadow,
         draw_static_meshes, draw_skinned_meshes, i](VkCommandBuffer) {
          if (!does_shadow) {
            return;
          }
          memcpy(renderer->GetShadowCameraUniformBuffer()->data_,
                 &renderer->GetShadowCameraUniformData(),
                 sizeof(ShadowMapMatricesUniformData));
          push_constant->cascade_index = i;

          // Phase 1: back-face culled (single-sided)
          pipeline->Bind(PipelineBindPointGraphics);
          draw_static_meshes(false);
          draw_skinned_meshes(false);

          // Phase 2: double-sided (no backface culling)
          pipeline_no_cull->Bind(PipelineBindPointGraphics);
          draw_static_meshes(true);
          draw_skinned_meshes(true);
        });
    if (shadow_depth.IsValid()) {
      graph.PassWritesDepth(shadow, shadow_depth);
    }
    graph.SetPassFramebuffer(
        shadow, pool.GetFramebuffer("shadow.fb." + std::to_string(i)));
    float shadow_size =
        static_cast<float>(renderer_->options().shadow_map_resolution.Get());
    graph.SetPassViewport(shadow, {shadow_size, shadow_size});
    graph.SetPassClearColor(shadow, {0, 0, 0, 1});
  }

  // Register the shadow depth output for downstream features
  if (shadow_depth.IsValid()) {
    registry.Register("ShadowDepth", shadow_depth);
  }
}

}  // namespace wiesel
