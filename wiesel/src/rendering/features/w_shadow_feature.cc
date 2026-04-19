
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

namespace {

// Gribb-Hartmann plane extraction from a combined view-projection matrix.
FrustumPlanes ExtractFrustumPlanesFromVP(const glm::mat4& m) {
  FrustumPlanes p;
  p.Left = glm::normalize(glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0],
                                    m[2][3] + m[2][0], m[3][3] + m[3][0]));
  p.Right = glm::normalize(glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0],
                                     m[2][3] - m[2][0], m[3][3] - m[3][0]));
  p.Bottom = glm::normalize(glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1],
                                      m[2][3] + m[2][1], m[3][3] + m[3][1]));
  p.Top = glm::normalize(glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1],
                                   m[2][3] - m[2][1], m[3][3] - m[3][1]));
  p.Near = glm::normalize(glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2],
                                    m[2][3] + m[2][2], m[3][3] + m[3][2]));
  p.Far = glm::normalize(glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2],
                                   m[2][3] - m[2][2], m[3][3] - m[3][2]));
  return p;
}

bool NeedsAlphaTest(const std::shared_ptr<Mesh>& mesh) {
  return mesh && mesh->has_transparency;
}

// Position + bone skinning attributes for the depth-only opaque shadow pipeline.
// Shares the Vertex3D vertex buffer layout but skips unused attribute fetches.
std::vector<VkVertexInputAttributeDescription>
GetOpaqueShadowAttributeDescriptions() {
  return {
      {0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex3D, ppos)},
      {7, 0, VK_FORMAT_R32G32B32A32_SINT,
       (uint32_t)offsetof(Vertex3D, bone_indices)},
      {8, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
       (uint32_t)offsetof(Vertex3D, bone_weights)},
  };
}

}  // namespace

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

  // Depth-only opaque shadow pipelines: no fragment shader, narrow vertex
  // input layout. Used for shadow casters that don't require alpha testing.
  auto vert_opaque = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/shadow_opaque.vert"});
  auto opaque_attrs = GetOpaqueShadowAttributeDescriptions();

  pipeline_opaque_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, false, true, true});
  pipeline_opaque_->SetRenderPass(render_pass_);
  pipeline_opaque_->SetVertexData(Vertex3D::GetBindingDescription(),
                                  opaque_attrs);
  pipeline_opaque_->AddPushConstant(push_constant_,
                                    VK_SHADER_STAGE_VERTEX_BIT);
  pipeline_opaque_->AddInputLayout(
      renderer_->GetDescriptorLayout("GeometryMesh"));
  pipeline_opaque_->AddInputLayout(
      renderer_->GetDescriptorLayout("GlobalShadow"));
  pipeline_opaque_->AddInputLayout(renderer_->GetDescriptorLayout("Bone"));
  pipeline_opaque_->AddShader(vert_opaque);
  pipeline_opaque_->Bake();

  pipeline_opaque_no_cull_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeNone, false, false, true, true});
  pipeline_opaque_no_cull_->SetRenderPass(render_pass_);
  pipeline_opaque_no_cull_->SetVertexData(Vertex3D::GetBindingDescription(),
                                          opaque_attrs);
  pipeline_opaque_no_cull_->AddPushConstant(push_constant_,
                                            VK_SHADER_STAGE_VERTEX_BIT);
  pipeline_opaque_no_cull_->AddInputLayout(
      renderer_->GetDescriptorLayout("GeometryMesh"));
  pipeline_opaque_no_cull_->AddInputLayout(
      renderer_->GetDescriptorLayout("GlobalShadow"));
  pipeline_opaque_no_cull_->AddInputLayout(
      renderer_->GetDescriptorLayout("Bone"));
  pipeline_opaque_no_cull_->AddShader(vert_opaque);
  pipeline_opaque_no_cull_->Bake();
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
  auto pipeline_opaque = pipeline_opaque_;
  auto pipeline_opaque_no_cull = pipeline_opaque_no_cull_;
  auto push_constant = push_constant_;
  bool does_shadow = ctx.camera.does_shadow_pass;

  // `alpha_test_only` selects the alpha-test (true) or opaque (false) caster
  // set; `double_sided_pass` picks back-face-culled vs double-sided geometry.
  auto draw_static_meshes = [&scenes, renderer](const FrustumPlanes& frustum,
                                                bool alpha_test_only,
                                                bool double_sided_pass) {
    scenes.ForEach<MeshRendererComponent, TransformComponent>(
        [&](Scene& scene, entt::entity entity) {
          auto& mr = scene.GetComponent<MeshRendererComponent>(entity);
          if (!mr.receive_shadows || !mr.enable_rendering ||
              !mr.model_handle.IsValid()) {
            return;
          }
          auto model_data =
              Engine::asset_manager().Get<Model>(mr.model_handle);
          if (!model_data || mr.mesh_index < 0 ||
              mr.mesh_index >=
                  static_cast<int32_t>(model_data->meshes.size())) {
            return;
          }
          auto& mesh = model_data->meshes[mr.mesh_index];
          if (NeedsAlphaTest(mesh) != alpha_test_only) {
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

  auto draw_skinned_meshes = [&scenes, renderer](const FrustumPlanes& frustum,
                                                 bool alpha_test_only,
                                                 bool double_sided_pass) {
    scenes.ForEach<SkinnedMeshRendererComponent, TransformComponent>(
        [&](Scene& scene, entt::entity entity) {
          auto& mr = scene.GetComponent<SkinnedMeshRendererComponent>(entity);
          if (!mr.receive_shadows || !mr.enable_rendering ||
              !mr.model_handle.IsValid()) {
            return;
          }
          auto model_data =
              Engine::asset_manager().Get<Model>(mr.model_handle);
          if (!model_data || mr.mesh_index < 0 ||
              mr.mesh_index >=
                  static_cast<int32_t>(model_data->meshes.size())) {
            return;
          }
          auto& mesh = model_data->meshes[mr.mesh_index];
          if (NeedsAlphaTest(mesh) != alpha_test_only) {
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
        [pipeline, pipeline_no_cull, pipeline_opaque, pipeline_opaque_no_cull,
         push_constant, renderer, does_shadow, draw_static_meshes,
         draw_skinned_meshes, i](VkCommandBuffer) {
          if (!does_shadow) {
            return;
          }
          memcpy(renderer->GetShadowCameraUniformBuffer()->data_,
                 &renderer->GetShadowCameraUniformData(),
                 sizeof(ShadowMapMatricesUniformData));
          push_constant->cascade_index = i;

          // Per-cascade frustum culling: each cascade's view-projection defines
          // a tighter frustum than the main camera, and using it here also fixes
          // the previous bug where casters outside the camera view were skipped.
          FrustumPlanes cascade_frustum = ExtractFrustumPlanesFromVP(
              renderer->GetCameraData()->shadow_map_cascades[i].ViewProjMatrix);

          // Opaque, single-sided: depth-only pipeline (no fragment shader).
          pipeline_opaque->Bind(PipelineBindPointGraphics);
          draw_static_meshes(cascade_frustum, false, false);
          draw_skinned_meshes(cascade_frustum, false, false);

          // Opaque, double-sided.
          pipeline_opaque_no_cull->Bind(PipelineBindPointGraphics);
          draw_static_meshes(cascade_frustum, false, true);
          draw_skinned_meshes(cascade_frustum, false, true);

          // Alpha-tested, single-sided: full shader with opacity sampling.
          pipeline->Bind(PipelineBindPointGraphics);
          draw_static_meshes(cascade_frustum, true, false);
          draw_skinned_meshes(cascade_frustum, true, false);

          // Alpha-tested, double-sided.
          pipeline_no_cull->Bind(PipelineBindPointGraphics);
          draw_static_meshes(cascade_frustum, true, true);
          draw_skinned_meshes(cascade_frustum, true, true);
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
