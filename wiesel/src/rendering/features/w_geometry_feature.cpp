
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_geometry_feature.hpp"
#include "rendering/w_renderer.hpp"
#include "rendering/w_pipeline.hpp"
#include "rendering/w_renderpass.hpp"
#include "rendering/w_mesh.hpp"
#include "scene/w_scene.hpp"

namespace Wiesel {

GeometryFeature::GeometryFeature(Ref<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Render pass
  render_pass_ =
      CreateReference<RenderPass>(PassType::Geometry, "Geometry RenderPass");
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
  render_pass_->AttachOutput(
      {.type = AttachmentTextureType::DepthStencil,
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
  }
  render_pass_->Bake();

  auto vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/internal_shaders/geometry_shader.vert"});
  auto frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/internal_shaders/geometry_shader.frag"});
  pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      renderer_->options().msaa_mode, CullModeBack,
      renderer_->options().wireframe_enabled, false});
  pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                           Vertex3D::GetAttributeDescriptions());
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->AddInputLayout(renderer_->GetGeometryMeshDescriptorLayout());
  pipeline_->AddInputLayout(renderer_->GetGlobalDescriptorLayout());
  pipeline_->AddShader(vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();
}

void GeometryFeature::SetupResources(RenderContext& ctx) {
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  auto& camera = ctx.camera;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  SamplingMode msaa = renderer.options().msaa_mode;
  bool use_msaa = msaa > SamplingMode::DISABLED;

  // G-buffer color attachments
  pool.SetTexture("geometry.view_pos", renderer.CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32G32B32A32_SFLOAT, msaa, true}));
  pool.SetTexture("geometry.world_pos", renderer.CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32G32B32A32_SFLOAT, msaa, true}));
  pool.SetTexture("geometry.depth", renderer.CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32_SFLOAT, msaa, true}));
  pool.SetTexture("geometry.normal", renderer.CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8G8B8A8_UNORM, msaa, true}));
  pool.SetTexture("geometry.albedo", renderer.CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8G8B8A8_UNORM, msaa, true}));
  pool.SetTexture("geometry.material", renderer.CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R16G16B16A16_SFLOAT, msaa, true}));
  pool.SetTexture("geometry.depth_stencil", renderer.CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::DepthStencil, 1,
       renderer.FindDepthFormat(), msaa, true}));

  if (use_msaa) {
    pool.SetTexture("geometry.view_pos_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R32G32B32A32_SFLOAT, SamplingMode::DISABLED, true}));
    pool.SetTexture("geometry.world_pos_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R32G32B32A32_SFLOAT, SamplingMode::DISABLED, true}));
    pool.SetTexture("geometry.depth_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R32_SFLOAT, SamplingMode::DISABLED, true}));
    pool.SetTexture("geometry.normal_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R8G8B8A8_UNORM, SamplingMode::DISABLED, true}));
    pool.SetTexture("geometry.albedo_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R8G8B8A8_UNORM, SamplingMode::DISABLED, true}));
    pool.SetTexture("geometry.material_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             VK_FORMAT_R16G16B16A16_SFLOAT, SamplingMode::DISABLED, true}));

    std::array<AttachmentTexture*, 13> textures = {
        pool.GetTexture("geometry.view_pos").get(),
        pool.GetTexture("geometry.world_pos").get(),
        pool.GetTexture("geometry.depth").get(),
        pool.GetTexture("geometry.normal").get(),
        pool.GetTexture("geometry.albedo").get(),
        pool.GetTexture("geometry.material").get(),
        pool.GetTexture("geometry.depth_stencil").get(),
        pool.GetTexture("geometry.view_pos_resolve").get(),
        pool.GetTexture("geometry.world_pos_resolve").get(),
        pool.GetTexture("geometry.depth_resolve").get(),
        pool.GetTexture("geometry.normal_resolve").get(),
        pool.GetTexture("geometry.albedo_resolve").get(),
        pool.GetTexture("geometry.material_resolve").get(),
    };
    pool.SetFramebuffer("geometry",
        render_pass_->CreateFramebuffer(0, textures, ctx.viewport_size));
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

    std::array<AttachmentTexture*, 7> textures = {
        pool.GetTexture("geometry.view_pos").get(),
        pool.GetTexture("geometry.world_pos").get(),
        pool.GetTexture("geometry.depth").get(),
        pool.GetTexture("geometry.normal").get(),
        pool.GetTexture("geometry.albedo").get(),
        pool.GetTexture("geometry.material").get(),
        pool.GetTexture("geometry.depth_stencil").get(),
    };
    pool.SetFramebuffer("geometry",
        render_pass_->CreateFramebuffer(0, textures, ctx.viewport_size));
  }

  // Geometry output descriptor (6 bindings, one per resolved G-buffer image)
  auto geo_output_desc = CreateReference<DescriptorSet>();
  geo_output_desc->SetLayout(renderer.GetGeometryOutputDescriptorLayout());
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
  auto& pool = ctx.resources;
  bool use_resolve = ctx.use_msaa_resolve;

  // Import G-buffer textures from the resource pool
  RGResource geo_view_pos = graph.ImportTexture(
      "GeoViewPos",
      use_resolve ? pool.GetTexture("geometry.view_pos_resolve")
                  : pool.GetTexture("geometry.view_pos"));
  RGResource geo_world_pos = graph.ImportTexture(
      "GeoWorldPos",
      use_resolve ? pool.GetTexture("geometry.world_pos_resolve")
                  : pool.GetTexture("geometry.world_pos"));
  RGResource geo_depth = graph.ImportTexture(
      "GeoDepth",
      use_resolve ? pool.GetTexture("geometry.depth_resolve")
                  : pool.GetTexture("geometry.depth"));
  RGResource geo_normal = graph.ImportTexture(
      "GeoNormal",
      use_resolve ? pool.GetTexture("geometry.normal_resolve")
                  : pool.GetTexture("geometry.normal"));
  RGResource geo_albedo = graph.ImportTexture(
      "GeoAlbedo",
      use_resolve ? pool.GetTexture("geometry.albedo_resolve")
                  : pool.GetTexture("geometry.albedo"));
  RGResource geo_material = graph.ImportTexture(
      "GeoMaterial",
      use_resolve ? pool.GetTexture("geometry.material_resolve")
                  : pool.GetTexture("geometry.material"));

  // Capture stable pointers for the deferred lambda execution.
  // Scene and Renderer are alive for the entire frame.
  auto* scene = &ctx.scene;
  auto renderer = renderer_;
  auto pipeline = pipeline_;

  uint32_t geo = graph.AddPass(
      "Geometry", render_pass_,
      [pipeline, scene, renderer](VkCommandBuffer) {
        pipeline->Bind(PipelineBindPointGraphics);
        for (const auto& entity :
             scene->GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
          auto& model = scene->GetComponent<ModelComponent>(entity);
          auto& transform = scene->GetComponent<TransformComponent>(entity);
          if (!model.enable_rendering || !model.model_handle)
            continue;
          renderer->DrawModel(model, transform, false);
        }
      });

  graph.PassWritesColor(geo, geo_view_pos);
  graph.PassWritesColor(geo, geo_world_pos);
  graph.PassWritesColor(geo, geo_depth);
  graph.PassWritesColor(geo, geo_normal);
  graph.PassWritesColor(geo, geo_albedo);
  graph.PassWritesColor(geo, geo_material);
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
}

}  // namespace Wiesel
