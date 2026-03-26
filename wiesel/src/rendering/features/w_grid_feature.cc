
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_grid_feature.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_scene.h"

namespace Wiesel {

GridFeature::GridFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  SamplingMode msaa = renderer_->options().msaa_mode;

  render_pass_ = std::make_shared<RenderPass>(PassType::ForwardTransparency,
                                              "Grid RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = msaa});
  render_pass_->AttachOutput({.type = AttachmentTextureType::DepthStencil,
                              .format = renderer_->FindDepthFormat(),
                              .msaa_mode = msaa});
  if (msaa > SamplingMode::DISABLED) {
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = renderer_->GetSwapChainImageFormat(),
                                .msaa_mode = SamplingMode::DISABLED});
  }
  render_pass_->Bake();

  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  auto grid_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/editor_grid.frag"});

  // Pipeline: no alpha blend, depth test on, depth write off (read-only depth)
  pipeline_ = std::make_shared<Pipeline>(
      PipelineProperties{msaa, CullModeNone, false, false, true, false});
  pipeline_->SetRenderPass(render_pass_);

  // Layout: set 0 = one UBO (GridUniformData)
  auto layout = std::make_shared<DescriptorSetLayout>();
  layout->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                     VK_SHADER_STAGE_FRAGMENT_BIT);
  layout->Bake();
  pipeline_->AddInputLayout(layout);

  pipeline_->AddShader(fullscreen_vert);
  pipeline_->AddShader(grid_frag);
  pipeline_->Bake();
}

void GridFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("GridFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  SamplingMode msaa = renderer.options().msaa_mode;
  bool use_msaa = msaa > SamplingMode::DISABLED;

  pool.SetTexture("grid.color",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       renderer.GetSwapChainImageFormat(), msaa, true}));

  if (use_msaa) {
    pool.SetTexture("grid.color_resolve",
                    renderer.CreateAttachmentTexture(
                        {rw, rh, AttachmentTextureType::Resolve, 1,
                         renderer.GetSwapChainImageFormat(),
                         SamplingMode::DISABLED, true}));
    std::array<AttachmentTexture*, 3> attachments{
        pool.GetTexture("grid.color").get(),
        pool.GetTexture("geometry.depth_stencil").get(),
        pool.GetTexture("grid.color_resolve").get()};
    pool.SetFramebuffer(
        "grid", render_pass_->CreateFramebuffer(0, attachments, {rw, rh}));
  } else {
    pool.SetTexture("grid.color_resolve", pool.GetTexture("grid.color"));
    std::array<AttachmentTexture*, 2> attachments{
        pool.GetTexture("grid.color").get(),
        pool.GetTexture("geometry.depth_stencil").get()};
    pool.SetFramebuffer(
        "grid", render_pass_->CreateFramebuffer(0, attachments, {rw, rh}));
  }

  // Grid output descriptor for composite
  auto output_desc = std::make_shared<DescriptorSet>();
  output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("grid.color_resolve")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  output_desc->Bake();
  pool.SetDescriptor("grid.output", output_desc);

  // Grid UBO
  pool.SetBuffer("grid.ubo",
                 renderer.CreateUniformBuffer(sizeof(GridUniformData)));

  // Grid draw descriptor (UBO for the grid shader)
  auto grid_desc = std::make_shared<DescriptorSet>();
  auto ubo_layout = std::make_shared<DescriptorSetLayout>();
  ubo_layout->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_FRAGMENT_BIT);
  ubo_layout->Bake();
  grid_desc->SetLayout(ubo_layout);
  grid_desc->AddUniformBuffer(0, pool.GetBuffer("grid.ubo"));
  grid_desc->Bake();
  pool.SetDescriptor("grid.draw", grid_desc);
}

void GridFeature::AddPasses(RenderGraph& graph,
                            RenderResourceRegistry& registry,
                            RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("GridFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto pipeline = pipeline_;

  RGResource grid_out =
      graph.ImportTexture("GridOut", pool->GetTexture("grid.color_resolve"));

  auto lighting_out = registry.Get("LightingOut");

  uint32_t pass = graph.AddPass(
      "Grid", render_pass_, [pipeline, pool, renderer](VkCommandBuffer) {
        // Update grid UBO with current camera data
        auto ubo = pool->GetBuffer("grid.ubo");
        auto cam_data = renderer->GetCameraData();
        if (ubo && ubo->data_ && cam_data) {
          GridUniformData data{};
          glm::mat4 vp = cam_data->projection * cam_data->view_matrix;
          data.inv_view_projection = glm::inverse(vp);
          data.view_projection = vp;
          data.camera_pos = glm::vec4(cam_data->position, 0.0f);
          memcpy(ubo->data_, &data, sizeof(GridUniformData));
        }

        pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(pipeline, {pool->GetDescriptor("grid.draw")});
      });

  graph.PassWritesColor(pass, grid_out);
  if (lighting_out.IsValid()) {
    graph.PassReadsTexture(pass, lighting_out);
  }
  graph.SetPassFramebuffer(pass, pool->GetFramebuffer("grid"));
  graph.SetPassViewport(pass, ctx.viewport_size);
  graph.SetPassClearColor(pass, {0, 0, 0, 0});

  registry.Register("GridOut", grid_out);
}

bool GridFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.show_grid;
}

}  // namespace Wiesel
