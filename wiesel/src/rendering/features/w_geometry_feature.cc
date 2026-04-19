
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_geometry_feature.h"
#include "asset/w_asset_manager.h"
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_instance_batcher.h"
#include "rendering/w_mesh_render_utils.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace wiesel {

GeometryFeature::GeometryFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Render pass
  render_pass_ =
      std::make_shared<RenderPass>(PassType::Geometry, "Geometry RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                              .msaa_mode = renderer_->options().msaa_mode});
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                              .msaa_mode = renderer_->options().msaa_mode});
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = VK_FORMAT_R32_SFLOAT,
                              .msaa_mode = renderer_->options().msaa_mode});
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = VK_FORMAT_R8G8B8A8_UNORM,
                              .msaa_mode = renderer_->options().msaa_mode});
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = VK_FORMAT_R8G8B8A8_UNORM,
                              .msaa_mode = renderer_->options().msaa_mode});
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                              .msaa_mode = renderer_->options().msaa_mode});
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = VK_FORMAT_R32_UINT,
                              .msaa_mode = renderer_->options().msaa_mode});
  render_pass_->AttachOutput({.type = AttachmentTextureType::DepthStencil,
                              .format = renderer_->FindDepthFormat(),
                              .msaa_mode = renderer_->options().msaa_mode});
  if (renderer_->options().msaa_mode > SamplingMode::DISABLED) {
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                .msaa_mode = SamplingMode::DISABLED});
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                .msaa_mode = SamplingMode::DISABLED});
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = VK_FORMAT_R32_SFLOAT,
                                .msaa_mode = SamplingMode::DISABLED});
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = VK_FORMAT_R8G8B8A8_UNORM,
                                .msaa_mode = SamplingMode::DISABLED});
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = VK_FORMAT_R8G8B8A8_UNORM,
                                .msaa_mode = SamplingMode::DISABLED});
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                .msaa_mode = SamplingMode::DISABLED});
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = VK_FORMAT_R32_UINT,
                                .msaa_mode = SamplingMode::DISABLED});
  }
  render_pass_->Bake();

  auto vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/geometry_shader.vert"});
  auto frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/geometry_shader.frag"});
  pipeline_ = std::make_shared<Pipeline>(
      PipelineProperties{renderer_->options().msaa_mode, CullModeBack,
                         renderer_->options().wireframe_enabled, false});
  pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                           Vertex3D::GetAttributeDescriptions());
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("GeometryMesh"));
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Global"));
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Bone"));
  pipeline_->AddShader(vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();

  pipeline_double_sided_ = std::make_shared<Pipeline>(
      PipelineProperties{renderer_->options().msaa_mode, CullModeNone,
                         renderer_->options().wireframe_enabled, false});
  pipeline_double_sided_->SetVertexData(Vertex3D::GetBindingDescription(),
                                        Vertex3D::GetAttributeDescriptions());
  pipeline_double_sided_->SetRenderPass(render_pass_);
  pipeline_double_sided_->AddInputLayout(
      renderer_->GetDescriptorLayout("GeometryMesh"));
  pipeline_double_sided_->AddInputLayout(
      renderer_->GetDescriptorLayout("Global"));
  pipeline_double_sided_->AddInputLayout(
      renderer_->GetDescriptorLayout("Bone"));
  pipeline_double_sided_->AddShader(vert);
  pipeline_double_sided_->AddShader(frag);
  pipeline_double_sided_->Bake();
}

void GeometryFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED();
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  auto& camera = ctx.camera;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  SamplingMode msaa = renderer.options().msaa_mode;
  bool use_msaa = msaa > SamplingMode::DISABLED;

  // G-buffer color attachments
  pool.SetTexture("geometry.view_pos",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R32G32B32A32_SFLOAT, msaa, true}));
  pool.SetTexture("geometry.world_pos",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R32G32B32A32_SFLOAT, msaa, true}));
  pool.SetTexture("geometry.depth",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R32_SFLOAT, msaa, true}));
  pool.SetTexture("geometry.normal",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R8G8B8A8_UNORM, msaa, true}));
  pool.SetTexture("geometry.albedo",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R8G8B8A8_UNORM, msaa, true}));
  pool.SetTexture("geometry.material",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R16G16B16A16_SFLOAT, msaa, true}));
  pool.SetTexture("geometry.entity_id",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R32_UINT, msaa, true}));
  pool.SetTexture("geometry.depth_stencil",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::DepthStencil, 1,
                       renderer.FindDepthFormat(), msaa, true}));

  if (use_msaa) {
    pool.SetTexture(
        "geometry.view_pos_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R32G32B32A32_SFLOAT, SamplingMode::DISABLED, true}));
    pool.SetTexture(
        "geometry.world_pos_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R32G32B32A32_SFLOAT, SamplingMode::DISABLED, true}));
    pool.SetTexture("geometry.depth_resolve",
                    renderer.CreateAttachmentTexture(
                        {rw, rh, AttachmentTextureType::Resolve, 1,
                         VK_FORMAT_R32_SFLOAT, SamplingMode::DISABLED, true}));
    pool.SetTexture(
        "geometry.normal_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R8G8B8A8_UNORM, SamplingMode::DISABLED, true}));
    pool.SetTexture(
        "geometry.albedo_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R8G8B8A8_UNORM, SamplingMode::DISABLED, true}));
    pool.SetTexture(
        "geometry.material_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R16G16B16A16_SFLOAT, SamplingMode::DISABLED, true}));
    pool.SetTexture("geometry.entity_id_resolve",
                    renderer.CreateAttachmentTexture(
                        {rw, rh, AttachmentTextureType::Resolve, 1,
                         VK_FORMAT_R32_UINT, SamplingMode::DISABLED, true}));

    std::array<AttachmentTexture*, 15> textures = {
        pool.GetTexture("geometry.view_pos").get(),
        pool.GetTexture("geometry.world_pos").get(),
        pool.GetTexture("geometry.depth").get(),
        pool.GetTexture("geometry.normal").get(),
        pool.GetTexture("geometry.albedo").get(),
        pool.GetTexture("geometry.material").get(),
        pool.GetTexture("geometry.entity_id").get(),
        pool.GetTexture("geometry.depth_stencil").get(),
        pool.GetTexture("geometry.view_pos_resolve").get(),
        pool.GetTexture("geometry.world_pos_resolve").get(),
        pool.GetTexture("geometry.depth_resolve").get(),
        pool.GetTexture("geometry.normal_resolve").get(),
        pool.GetTexture("geometry.albedo_resolve").get(),
        pool.GetTexture("geometry.material_resolve").get(),
        pool.GetTexture("geometry.entity_id_resolve").get(),
    };
    pool.SetFramebuffer("geometry", render_pass_->CreateFramebuffer(
                                        0, textures, ctx.viewport_size));
  } else {
    // Without MSAA the resolve aliases point to the same textures
    pool.SetTexture("geometry.view_pos_resolve",
                    pool.GetTexture("geometry.view_pos"));
    pool.SetTexture("geometry.world_pos_resolve",
                    pool.GetTexture("geometry.world_pos"));
    pool.SetTexture("geometry.depth_resolve",
                    pool.GetTexture("geometry.depth"));
    pool.SetTexture("geometry.normal_resolve",
                    pool.GetTexture("geometry.normal"));
    pool.SetTexture("geometry.albedo_resolve",
                    pool.GetTexture("geometry.albedo"));
    pool.SetTexture("geometry.material_resolve",
                    pool.GetTexture("geometry.material"));
    pool.SetTexture("geometry.entity_id_resolve",
                    pool.GetTexture("geometry.entity_id"));

    std::array<AttachmentTexture*, 8> textures = {
        pool.GetTexture("geometry.view_pos").get(),
        pool.GetTexture("geometry.world_pos").get(),
        pool.GetTexture("geometry.depth").get(),
        pool.GetTexture("geometry.normal").get(),
        pool.GetTexture("geometry.albedo").get(),
        pool.GetTexture("geometry.material").get(),
        pool.GetTexture("geometry.entity_id").get(),
        pool.GetTexture("geometry.depth_stencil").get(),
    };
    pool.SetFramebuffer("geometry", render_pass_->CreateFramebuffer(
                                        0, textures, ctx.viewport_size));
  }

  // Geometry output descriptor (6 bindings, one per resolved G-buffer image)
  auto geo_output_desc = std::make_shared<DescriptorSet>();
  geo_output_desc->SetLayout(renderer.GetDescriptorLayout("GeometryOutput"));
  geo_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("geometry.view_pos_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  geo_output_desc->AddCombinedImageSampler(
      1, pool.GetTexture("geometry.world_pos_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  geo_output_desc->AddCombinedImageSampler(
      2, pool.GetTexture("geometry.depth_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  geo_output_desc->AddCombinedImageSampler(
      3, pool.GetTexture("geometry.normal_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  geo_output_desc->AddCombinedImageSampler(
      4, pool.GetTexture("geometry.albedo_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  geo_output_desc->AddCombinedImageSampler(
      5, pool.GetTexture("geometry.material_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  geo_output_desc->Bake();
  pool.SetDescriptor("geometry.output", geo_output_desc);

  // Global descriptor for the camera
  pool.SetDescriptor("GlobalDescriptor",
                     renderer.CreateGlobalDescriptors(camera));
}

void GeometryFeature::AddPasses(RenderGraph& graph,
                                RenderResourceRegistry& registry,
                                RenderContext& ctx) {
  PROFILE_ZONE_SCOPED();
  auto& pool = ctx.resources;
  bool use_resolve = ctx.use_msaa_resolve;

  // Import G-buffer textures from the resource pool
  RGResource geo_view_pos = graph.ImportTexture(
      "GeoViewPos", use_resolve ? pool.GetTexture("geometry.view_pos_resolve")
                                : pool.GetTexture("geometry.view_pos"));
  RGResource geo_world_pos = graph.ImportTexture(
      "GeoWorldPos", use_resolve ? pool.GetTexture("geometry.world_pos_resolve")
                                 : pool.GetTexture("geometry.world_pos"));
  RGResource geo_depth = graph.ImportTexture(
      "GeoDepth", use_resolve ? pool.GetTexture("geometry.depth_resolve")
                              : pool.GetTexture("geometry.depth"));
  RGResource geo_normal = graph.ImportTexture(
      "GeoNormal", use_resolve ? pool.GetTexture("geometry.normal_resolve")
                               : pool.GetTexture("geometry.normal"));
  RGResource geo_albedo = graph.ImportTexture(
      "GeoAlbedo", use_resolve ? pool.GetTexture("geometry.albedo_resolve")
                               : pool.GetTexture("geometry.albedo"));
  RGResource geo_material = graph.ImportTexture(
      "GeoMaterial", use_resolve ? pool.GetTexture("geometry.material_resolve")
                                 : pool.GetTexture("geometry.material"));
  RGResource geo_entity_id = graph.ImportTexture(
      "GeoEntityId", use_resolve ? pool.GetTexture("geometry.entity_id_resolve")
                                 : pool.GetTexture("geometry.entity_id"));

  // Capture stable pointers for the deferred lambda execution.
  // Scenes and Renderer are alive for the entire frame.
  MultiScene& scenes = ctx.scenes;
  auto renderer = renderer_;
  auto pipeline = pipeline_;
  auto pipeline_ds = pipeline_double_sided_;

  // Walks every enabled, opaque, unculled static mesh in the scene
  // once and forwards it to `callback` along with the resolved
  // double-sided flag. Callers bin into the appropriate batcher rather
  // than iterating twice.
  auto for_each_static = [&scenes, renderer](auto&& callback) {
    PROFILE_ZONE_SCOPED_N("for_each_static");
    const FrustumPlanes& frustum = renderer->GetCameraData()->planes;
    uint32_t visited = 0;
    scenes.ForEach<MeshRendererComponent, TransformComponent>(
        [&](Scene& scene, uint8_t scene_idx, entt::entity entity) {
          visited++;
          auto& mr = scene.GetComponent<MeshRendererComponent>(entity);
          if (!mr.enable_rendering || !mr.model_handle.IsValid()) {
            return;
          }
          Renderer::MeshDrawPrep prep;
          if (!renderer->PrepareMesh(mr, prep)) {
            return;
          }
          if (prep.mesh->has_transparency) {
            return;
          }
          auto& transform = scene.GetComponent<TransformComponent>(entity);
          if (FrustumCullMeshDirect(frustum, *prep.mesh,
                                    transform.GetTransformMatrix())) {
            return;
          }
          const bool double_sided = IsMaterialDoubleSided(prep.material);
          callback(mr, transform, prep, entity, scene_idx, double_sided);
        });
    ZoneValue(visited);
  };

  auto draw_skinned_meshes = [&scenes, renderer](bool double_sided_pass) {
    const FrustumPlanes& frustum = renderer->GetCameraData()->planes;
    scenes.ForEach<SkinnedMeshRendererComponent, TransformComponent>(
        [&](Scene& scene, entt::entity entity) {
          auto& mr = scene.GetComponent<SkinnedMeshRendererComponent>(entity);
          if (!mr.enable_rendering || !mr.model_handle.IsValid()) {
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
          renderer->DrawSkinnedMeshRenderer(mr, *draw_transform, skel, false,
                                            false, entity);
        });
  };

  uint32_t geo = graph.AddPass(
      "Geometry", render_pass_,
      [pipeline, pipeline_ds, renderer, for_each_static,
       draw_skinned_meshes](VkCommandBuffer cmd) {
        auto global_desc =
            renderer->GetCameraData()->resource_pool->GetDescriptor(
                "GlobalDescriptor");
        auto bone_desc = renderer->GetIdentityBoneDescriptor();

        MeshInstanceBatcher batcher_single(renderer.get());
        MeshInstanceBatcher batcher_double(renderer.get());
        {
          PROFILE_ZONE_SCOPED_N("Collect static meshes");
          for_each_static([&](MeshRendererComponent& mr,
                              const TransformComponent& transform,
                              Renderer::MeshDrawPrep& prep,
                              entt::entity entity, uint8_t scene_idx,
                              bool double_sided) {
            MatricesUniformData data =
                BuildInstanceData(mr, transform, entity, scene_idx);
            auto& target = double_sided ? batcher_double : batcher_single;
            target.Add(prep.mesh, prep.material, prep.geometry_descriptor,
                       data);
          });
        }

        auto submit_phase = [&](Pipeline* pl, MeshInstanceBatcher& batcher,
                                bool double_sided) {
          PROFILE_ZONE_SCOPED_N("Geometry Pass Phase");
          ZoneText(double_sided ? "double-sided" : "single-sided", 12);
          pl->Bind(PipelineBindPointGraphics, cmd);
          batcher.Flush(cmd, pl, global_desc, bone_desc);
          draw_skinned_meshes(double_sided);
        };

        submit_phase(pipeline.get(), batcher_single, false);
        submit_phase(pipeline_ds.get(), batcher_double, true);
      });

  graph.PassWritesColor(geo, geo_view_pos);
  graph.PassWritesColor(geo, geo_world_pos);
  graph.PassWritesColor(geo, geo_depth);
  graph.PassWritesColor(geo, geo_normal);
  graph.PassWritesColor(geo, geo_albedo);
  graph.PassWritesColor(geo, geo_material);
  graph.PassWritesColor(geo, geo_entity_id);
  graph.SetPassFramebuffer(geo, pool.GetFramebuffer("geometry"));
  graph.SetPassViewport(geo, ctx.viewport_size);
  graph.SetPassClearColor(geo, {0, 0, 0, 0});

  // Register outputs for downstream features
  registry.Register("GeoViewPos", geo_view_pos);
  registry.Register("GeoWorldPos", geo_world_pos);
  registry.Register("GeoDepth", geo_depth);
  registry.Register("GeoNormal", geo_normal);
  registry.Register("GeoAlbedo", geo_albedo);
  registry.Register("GeoMaterial", geo_material);
  registry.Register("GeoEntityId", geo_entity_id);
}

}  // namespace wiesel
