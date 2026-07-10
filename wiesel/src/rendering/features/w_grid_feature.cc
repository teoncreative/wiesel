
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
#include "scene/w_scene.h"

namespace wiesel {

GridFeature::GridFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  SamplingMode msaa = renderer_->options().msaa_mode;

  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  auto grid_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/editor_grid.frag"});

  // Pipeline: no alpha blend, depth test on, depth write off (read-only depth)
  pipeline_ = std::make_shared<Pipeline>(
      PipelineProperties{msaa, CullModeNone, false, false, true, false});
  pipeline_->AddColorAttachment(renderer_->GetSwapChainImageFormat());
  pipeline_->SetDepthAttachment(renderer_->FindDepthFormat());

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
  } else {
    pool.SetTexture("grid.color_resolve", pool.GetTexture("grid.color"));
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
  pool.SetBuffer("grid.ubo", renderer.CreateUniformBuffer(
                                 "grid.ubo", sizeof(GridUniformData)));

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
  SamplingMode msaa = renderer_->options().msaa_mode;
  bool use_msaa = msaa > SamplingMode::DISABLED;

  RGResource grid_color =
      graph.ImportTexture("GridColor", pool->GetTexture("grid.color"));
  RGResource grid_resolve =
      use_msaa ? graph.ImportTexture("GridOut",
                                     pool->GetTexture("grid.color_resolve"))
               : grid_color;
  RGResource grid_depth = graph.ImportTexture(
      "GridDepth", pool->GetTexture("geometry.depth_stencil"));

  auto lighting_out = registry.Get("LightingOut");

  uint32_t pass = graph.AddPass(
      "Grid", [pipeline, pool, renderer](VkCommandBuffer) {
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

        pipeline->Bind();
        renderer->DrawFullscreen(pipeline, {pool->GetDescriptor("grid.draw")});
      });

  if (use_msaa) {
    graph.PassWritesColor(pass, grid_color, grid_resolve);
  } else {
    graph.PassWritesColor(pass, grid_color);
  }
  graph.PassWritesDepthLoad(pass, grid_depth);
  if (lighting_out.IsValid()) {
    graph.PassReadsTexture(pass, lighting_out);
  }
  graph.SetPassViewport(pass, ctx.viewport_size);
  graph.SetPassClearColor(pass, {0, 0, 0, 0});

  registry.Register("GridOut", grid_resolve);
}

bool GridFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.show_grid;
}

}  // namespace wiesel
