
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_shadow_feature.hpp"
#include "rendering/w_renderer.hpp"
#include "rendering/w_pipeline.hpp"
#include "rendering/w_renderpass.hpp"
#include "rendering/w_mesh.hpp"
#include "scene/w_scene.hpp"

namespace Wiesel {

ShadowFeature::ShadowFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  render_pass_ =
      std::make_shared<RenderPass>(PassType::Shadow, "Shadow RenderPass");
  render_pass_->AttachOutput(
      {.type = AttachmentTextureType::DepthStencil,
       .format = renderer_->FindDepthFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  auto vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/shadow_shader.vert"});
  auto frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/shadow_shader.frag"});
  pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, true, true});
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                           Vertex3D::GetAttributeDescriptions());
  push_constant_ = std::make_shared<ShadowPipelinePushConstant>();
  pipeline_->AddPushConstant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT);
  pipeline_->AddInputLayout(renderer_->GetGeometryMeshDescriptorLayout());
  pipeline_->AddInputLayout(renderer_->GetGlobalShadowDescriptorLayout());
  pipeline_->AddInputLayout(renderer_->GetBoneDescriptorLayout());
  pipeline_->AddShader(vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();
}

void ShadowFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("ShadowFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  auto& camera = ctx.camera;

  // Layered depth texture for all cascades
  pool.SetTexture("shadow.depth_stencil", renderer.CreateAttachmentTexture(
      {WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM,
       AttachmentTextureType::DepthStencil, 1, renderer.FindDepthFormat(),
       SamplingMode::DISABLED, true, WIESEL_SHADOW_CASCADE_COUNT}));

  auto shadow_depth = pool.GetTexture("shadow.depth_stencil");

  // Array view spanning all cascade layers (used by the global descriptor / lighting shader)
  pool.SetImageView("ShadowDepthViewArray", renderer.CreateImageView(
      shadow_depth, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0,
      WIESEL_SHADOW_CASCADE_COUNT));

  // Per-cascade image views and framebuffers
  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    auto cascade_view = renderer.CreateImageView(
        shadow_depth, VK_IMAGE_VIEW_TYPE_2D, i);
    std::array<ImageView*, 1> views = {
        cascade_view.get(),
    };
    pool.SetImageView("ShadowDepthView" + std::to_string(i), cascade_view);
    pool.SetFramebuffer("shadow.fb." + std::to_string(i),
        render_pass_->CreateFramebuffer(
            views, {WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM}));
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
  auto* scene = &ctx.scene;
  auto renderer = renderer_;
  auto pipeline = pipeline_;
  auto push_constant = push_constant_;
  bool does_shadow = ctx.camera.does_shadow_pass;

  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    uint32_t shadow = graph.AddPass(
        "Shadow " + std::to_string(i), render_pass_,
        [pipeline, push_constant, scene, renderer, does_shadow,
         i](VkCommandBuffer) {
          if (!does_shadow) {
            return;
          }
          memcpy(renderer->GetShadowCameraUniformBuffer()->data_,
                 &renderer->GetShadowCameraUniformData(),
                 sizeof(ShadowMapMatricesUniformData));
          push_constant->cascade_index = i;
          pipeline->Bind(PipelineBindPointGraphics);
          for (const auto& entity :
               scene
                   ->GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
            auto& model = scene->GetComponent<ModelComponent>(entity);
            auto& transform =
                scene->GetComponent<TransformComponent>(entity);
            if (!model.receive_shadows || !model.enable_rendering ||
                !model.model_handle)
              continue;
            renderer->DrawModel(model, transform, true);
          }
        });
    if (shadow_depth.IsValid()) {
      graph.PassWritesDepth(shadow, shadow_depth);
    }
    graph.SetPassFramebuffer(
        shadow, pool.GetFramebuffer("shadow.fb." + std::to_string(i)));
    graph.SetPassViewport(shadow,
                          {WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM});
    graph.SetPassClearColor(shadow, {0, 0, 0, 1});
  }

  // Register the shadow depth output for downstream features
  if (shadow_depth.IsValid()) {
    registry.Register("ShadowDepth", shadow_depth);
  }
}

}  // namespace Wiesel
