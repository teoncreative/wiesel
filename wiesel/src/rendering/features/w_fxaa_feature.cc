
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_fxaa_feature.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_scene.h"

namespace wiesel {

FXAAFeature::FXAAFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Postprocess render pass (1 color, no MSAA)
  render_pass_ = std::make_shared<RenderPass>(PassType::PostProcess,
                                              "PostProcess RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  push_constants_ = std::make_shared<FxaaPushConstants>();
  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  auto frag = renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL,
                                       "main", ShaderSourceSource,
                                       "engine://shaders/fxaa.frag"});
  pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, false, false, false});
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Present"));
  pipeline_->AddPushConstant(push_constants_, VK_SHADER_STAGE_FRAGMENT_BIT);
  pipeline_->AddShader(fullscreen_vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();
}

bool FXAAFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.renderer.options().aa_mode == AntiAliasingMode::FXAA;
}

void FXAAFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("FXAAFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  pool.SetTexture("fxaa.color", renderer.CreateAttachmentTexture(
                                    {rw, rh, AttachmentTextureType::Offscreen,
                                     1, renderer.GetSwapChainImageFormat(),
                                     SamplingMode::DISABLED, true}));

  {
    std::array<AttachmentTexture*, 1> att{pool.GetTexture("fxaa.color").get()};
    pool.SetFramebuffer("fxaa",
                        render_pass_->CreateFramebuffer(0, att, {rw, rh}));
  }

  // FXAA input: reads PipelineOutput (whatever the previous feature set)
  auto fxaa_input_desc = std::make_shared<DescriptorSet>();
  fxaa_input_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  fxaa_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  fxaa_input_desc->Bake();
  pool.SetDescriptor("fxaa.input", fxaa_input_desc);

  // FXAA output descriptor: reads fxaa.color
  auto fxaa_output_desc = std::make_shared<DescriptorSet>();
  fxaa_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  fxaa_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("fxaa.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  fxaa_output_desc->Bake();
  pool.SetDescriptor("fxaa.output", fxaa_output_desc);

  // Update pipeline output for the next feature in the chain
  pool.SetTexture("PipelineOutput", pool.GetTexture("fxaa.color"));
  pool.SetDescriptor("PipelineOutputDescriptor", fxaa_output_desc);
}

void FXAAFeature::AddPasses(RenderGraph& graph,
                            RenderResourceRegistry& registry,
                            RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("FXAAFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto push_constants = push_constants_;
  glm::vec2 viewport_size = ctx.viewport_size;

  // Import FXAA output texture from pool
  RGResource fxaa_out =
      graph.ImportTexture("FXAA", pool->GetTexture("fxaa.color"));

  // Get PipelineOutput from registry (set by previous feature)
  auto pipeline_input = registry.Get("PipelineOutput");

  auto pipeline = pipeline_;
  uint32_t fxaa_pass = graph.AddPass(
      "FXAA", render_pass_,
      [pool, renderer, pipeline, push_constants,
       viewport_size](VkCommandBuffer) {
        push_constants->inverse_screen_size = {1.0f / viewport_size.x,
                                               1.0f / viewport_size.y};
        pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(pipeline, {pool->GetDescriptor("fxaa.input")});
      });

  graph.PassReadsTexture(fxaa_pass, pipeline_input);
  graph.PassWritesColor(fxaa_pass, fxaa_out);
  graph.SetPassFramebuffer(fxaa_pass, pool->GetFramebuffer("fxaa"));
  graph.SetPassViewport(fxaa_pass, ctx.viewport_size);
  graph.SetPassClearColor(fxaa_pass, {0, 0, 0, 0});

  // Update PipelineOutput for the next feature in the chain
  registry.Register("PipelineOutput", fxaa_out);
}

}  // namespace wiesel
