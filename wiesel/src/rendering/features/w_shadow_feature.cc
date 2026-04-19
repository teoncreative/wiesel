
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
#include "rendering/w_mesh_instance_batcher.h"
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

  pool.SetTexture(
      "shadow.depth_stencil",
      renderer.CreateAttachmentTexture(
          {shadow_dim, shadow_dim, AttachmentTextureType::DepthStencil, 1,
           renderer.FindDepthFormat(), SamplingMode::DISABLED, true,
           WIESEL_SHADOW_CASCADE_COUNT}));

  auto shadow_depth = pool.GetTexture("shadow.depth_stencil");

  pool.SetImageView(
      "ShadowDepthViewArray",
      renderer.CreateImageView(shadow_depth, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0,
                               WIESEL_SHADOW_CASCADE_COUNT));

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

  pool.SetDescriptor("ShadowGlobalDescriptor",
                     renderer.CreateShadowGlobalDescriptors(camera));
}

void ShadowFeature::AddPasses(RenderGraph& graph,
                              RenderResourceRegistry& registry,
                              RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("ShadowFeature::AddPasses");
  auto& pool = ctx.resources;

  RGResource shadow_depth;
  auto shadow_tex = pool.GetTexture("shadow.depth_stencil");
  if (shadow_tex) {
    shadow_depth = graph.ImportTexture("ShadowDepth", shadow_tex);
  }

  MultiScene& scenes = ctx.scenes;
  auto renderer = renderer_;
  auto pipeline = pipeline_;
  auto pipeline_no_cull = pipeline_no_cull_;
  auto pipeline_opaque = pipeline_opaque_;
  auto pipeline_opaque_no_cull = pipeline_opaque_no_cull_;
  auto push_constant = push_constant_;
  bool does_shadow = ctx.camera.does_shadow_pass;

  // Scene scan + PrepareMesh + world-AABB + classification is cascade
  // independent, so we collect it once per frame here.
  struct PreppedStatic {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    std::shared_ptr<DescriptorSet> shadow_descriptor;
    MatricesUniformData data;
    AABB world_bounds;
    bool alpha_test;
    bool double_sided;
  };
  struct PreppedSkinned {
    SkinnedMeshRendererComponent* mr;
    const TransformComponent* transform;
    const SkeletalAnimRuntime* skel;
    AABB world_bounds;
    bool skip_cull;
    bool alpha_test;
    bool double_sided;
  };

  auto prepped_static = std::make_shared<std::vector<PreppedStatic>>();
  auto prepped_skinned = std::make_shared<std::vector<PreppedSkinned>>();

  if (does_shadow) {
    PROFILE_ZONE_SCOPED_N("ShadowFeature::CollectCasters");
    scenes.ForEach<MeshRendererComponent, TransformComponent>(
        [&](Scene& scene, uint8_t scene_idx, entt::entity entity) {
          auto& mr = scene.GetComponent<MeshRendererComponent>(entity);
          if (!mr.receive_shadows || !mr.enable_rendering ||
              !mr.model_handle.IsValid()) {
            return;
          }
          Renderer::MeshDrawPrep prep;
          if (!renderer->PrepareMesh(mr, prep)) {
            return;
          }
          auto& transform = scene.GetComponent<TransformComponent>(entity);
          PreppedStatic p;
          p.mesh = prep.mesh;
          p.material = prep.material;
          p.shadow_descriptor = prep.shadow_descriptor;
          p.data = BuildInstanceData(mr, transform, entity, scene_idx);
          p.world_bounds =
              prep.mesh->bounds.Transformed(transform.GetTransformMatrix());
          p.alpha_test = NeedsAlphaTest(prep.mesh);
          p.double_sided = IsMaterialDoubleSided(prep.material);
          prepped_static->push_back(std::move(p));
        });

    scenes.ForEach<SkinnedMeshRendererComponent, TransformComponent>(
        [&](Scene& scene, entt::entity entity) {
          auto& mr = scene.GetComponent<SkinnedMeshRendererComponent>(entity);
          if (!mr.receive_shadows || !mr.enable_rendering ||
              !mr.model_handle.IsValid()) {
            return;
          }
          Renderer::MeshDrawPrep prep;
          if (!renderer->PrepareMesh(mr, prep)) {
            return;
          }
          const TransformComponent* draw_transform =
              &scene.GetComponent<TransformComponent>(entity);
          const SkeletalAnimRuntime* skel = nullptr;
          if (!ResolveSkeletonRoot(scene, mr, draw_transform, skel)) {
            return;
          }
          PreppedSkinned p;
          p.mr = &mr;
          p.transform = draw_transform;
          p.skel = skel;
          p.skip_cull = !(skel && skel->rest_pose_bounds.Valid());
          if (!p.skip_cull) {
            p.world_bounds = skel->rest_pose_bounds.Transformed(
                draw_transform->GetTransformMatrix());
            if (skel->max_bone_reach > 0.0f) {
              glm::vec3 expand(skel->max_bone_reach);
              p.world_bounds.min -= expand;
              p.world_bounds.max += expand;
            }
          }
          p.alpha_test = NeedsAlphaTest(prep.mesh);
          p.double_sided = IsMaterialDoubleSided(prep.material);
          prepped_skinned->push_back(std::move(p));
        });
  }

  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    uint32_t shadow = graph.AddPass(
        "Shadow " + std::to_string(i), render_pass_,
        [pipeline, pipeline_no_cull, pipeline_opaque, pipeline_opaque_no_cull,
         push_constant, renderer, does_shadow, prepped_static, prepped_skinned,
         i](VkCommandBuffer cmd) {
          if (!does_shadow) {
            return;
          }
          std::memcpy(renderer->GetShadowCameraUniformBuffer()->data_,
                      &renderer->GetShadowCameraUniformData(),
                      sizeof(ShadowMapMatricesUniformData));
          push_constant->cascade_index = i;

          FrustumPlanes cascade_frustum = ExtractFrustumPlanesFromVP(
              renderer->GetCameraData()->shadow_map_cascades[i].ViewProjMatrix);

          auto global_desc =
              renderer->GetCameraData()->resource_pool->GetDescriptor(
                  "ShadowGlobalDescriptor");
          auto bone_desc = renderer->GetIdentityBoneDescriptor();

          // One batcher + one skinned list per pipeline bucket. Indexed as
          // [alpha_test][double_sided] so (0,0)=opaque_cull, (0,1)=opaque_nocull,
          // (1,0)=alpha_cull, (1,1)=alpha_nocull.
          struct SkinnedDraw {
            SkinnedMeshRendererComponent* mr;
            const TransformComponent* transform;
            const SkeletalAnimRuntime* skel;
          };
          std::array<std::array<MeshInstanceBatcher, 2>, 2> batchers{{
              {MeshInstanceBatcher(renderer.get()),
               MeshInstanceBatcher(renderer.get())},
              {MeshInstanceBatcher(renderer.get()),
               MeshInstanceBatcher(renderer.get())},
          }};
          std::array<std::array<std::vector<SkinnedDraw>, 2>, 2> skinned{};

          for (auto& p : *prepped_static) {
            if (cascade_frustum.IsBoxOutside(p.world_bounds.min,
                                             p.world_bounds.max)) {
              continue;
            }
            batchers[p.alpha_test][p.double_sided].Add(
                p.mesh, p.material, p.shadow_descriptor, p.data);
          }

          for (auto& p : *prepped_skinned) {
            if (!p.skip_cull &&
                cascade_frustum.IsBoxOutside(p.world_bounds.min,
                                             p.world_bounds.max)) {
              continue;
            }
            skinned[p.alpha_test][p.double_sided].push_back(
                {p.mr, p.transform, p.skel});
          }

          auto submit_bucket = [&](Pipeline* pl, bool alpha_test,
                                   bool double_sided) {
            auto& batcher = batchers[alpha_test][double_sided];
            auto& skinned_list = skinned[alpha_test][double_sided];
            if (batcher.Empty() && skinned_list.empty()) {
              return;
            }
            pl->Bind(PipelineBindPointGraphics, cmd);
            batcher.Flush(cmd, pl, global_desc, bone_desc);
            // Skinned meshes aren't instanced (per-entity bone descriptors)
            // so they continue on the per-entity draw path.
            for (auto& d : skinned_list) {
              renderer->DrawSkinnedMeshRenderer(*d.mr, *d.transform, d.skel,
                                                true);
            }
          };

          submit_bucket(pipeline_opaque.get(), false, false);
          submit_bucket(pipeline_opaque_no_cull.get(), false, true);
          submit_bucket(pipeline.get(), true, false);
          submit_bucket(pipeline_no_cull.get(), true, true);
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

  if (shadow_depth.IsValid()) {
    registry.Register("ShadowDepth", shadow_depth);
  }
}

}  // namespace wiesel
