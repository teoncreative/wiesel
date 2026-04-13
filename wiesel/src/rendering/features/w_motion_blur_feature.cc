
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_motion_blur_feature.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_scene.h"

namespace wiesel {

MotionBlurFeature::MotionBlurFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Postprocess render pass (1 color, no MSAA)
  render_pass_ = std::make_shared<RenderPass>(PassType::PostProcess,
                                              "PostProcess RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  // Pipeline (2-input layout + global layout + push constants)
  push_constants_ = std::make_shared<MotionBlurPushConstants>();
  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  auto frag = renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL,
                                       "main", ShaderSourceSource,
                                       "engine://shaders/motion_blur.frag"});
  pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, false, false, false});
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->AddInputLayout(
      renderer_->GetDescriptorLayout("Postprocess2Input"));
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Global"));
  pipeline_->AddPushConstant(push_constants_, VK_SHADER_STAGE_FRAGMENT_BIT);
  pipeline_->AddShader(fullscreen_vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();
}

bool MotionBlurFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.renderer.options().motion_blur_enabled;
}

void MotionBlurFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("MotionBlurFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  pool.SetTexture(
      "motion_blur.color",
      renderer.CreateAttachmentTexture(
          {rw, rh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));

  pool.SetFramebuffer(
      "motion_blur",
      render_pass_->CreateFramebuffer(
          0, {pool.GetTexture("motion_blur.color").get()}, {rw, rh}));

  // Motion blur input: reads PipelineOutput + geometry world pos (2 inputs)
  auto motion_blur_input_desc = std::make_shared<DescriptorSet>();
  motion_blur_input_desc->SetLayout(
      renderer.GetDescriptorLayout("Postprocess2Input"));
  motion_blur_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  motion_blur_input_desc->AddCombinedImageSampler(
      1, pool.GetTexture("geometry.world_pos_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  motion_blur_input_desc->Bake();
  pool.SetDescriptor("motion_blur.input", motion_blur_input_desc);

  // Motion blur output descriptor: reads motion_blur.color
  auto motion_blur_output_desc = std::make_shared<DescriptorSet>();
  motion_blur_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  motion_blur_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("motion_blur.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  motion_blur_output_desc->Bake();
  pool.SetDescriptor("motion_blur.output", motion_blur_output_desc);

  // Update pipeline output for the next feature in the chain
  pool.SetTexture("PipelineOutput", pool.GetTexture("motion_blur.color"));
  pool.SetDescriptor("PipelineOutputDescriptor", motion_blur_output_desc);
}

void MotionBlurFeature::AddPasses(RenderGraph& graph,
                                  RenderResourceRegistry& registry,
                                  RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("MotionBlurFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto push_constants = push_constants_;

  // Import motion blur output texture from pool
  RGResource motion_blur_out =
      graph.ImportTexture("MotionBlur", pool->GetTexture("motion_blur.color"));

  // Get PipelineOutput and geometry world pos from registry
  auto pipeline_input = registry.Get("PipelineOutput");
  auto geo_world_pos = registry.Get("GeoWorldPos");

  auto pipeline = pipeline_;
  uint32_t motion_blur = graph.AddPass(
      "MotionBlur", render_pass_,
      [pool, renderer, pipeline, push_constants](VkCommandBuffer) {
        auto& s = renderer->options();
        push_constants->strength = s.motion_blur_strength;
        push_constants->num_samples = s.motion_blur_samples;
        pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(pipeline,
                                 {pool->GetDescriptor("motion_blur.input"),
                                  pool->GetDescriptor("GlobalDescriptor")});
      });

  graph.PassReadsTexture(motion_blur, pipeline_input);
  graph.PassReadsTexture(motion_blur, geo_world_pos);
  graph.PassWritesColor(motion_blur, motion_blur_out);
  graph.SetPassFramebuffer(motion_blur, pool->GetFramebuffer("motion_blur"));
  graph.SetPassViewport(motion_blur, ctx.viewport_size);
  graph.SetPassClearColor(motion_blur, {0, 0, 0, 0});

  // Update PipelineOutput for the next feature in the chain
  registry.Register("PipelineOutput", motion_blur_out);
}

}  // namespace wiesel
