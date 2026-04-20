
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
#include "rendering/w_framebuffer.h"
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

void MotionBlurFeature::SetupResources(RenderContext& /*ctx*/) {}

void MotionBlurFeature::AddPasses(RenderGraph& graph,
                                  RenderResourceRegistry& registry,
                                  RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("MotionBlurFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto push_constants = push_constants_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  RGResource motion_blur_out = graph.DeclareTransient(RGTextureDesc{
      .name = "motion_blur.color",
      .width = rw,
      .height = rh,
      .format = renderer->GetSwapChainImageFormat(),
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true});

  RGResource pipeline_input = registry.Get("PipelineOutput");
  RGResource geo_world_pos = registry.Get("GeoWorldPos");

  auto pipeline = pipeline_;
  uint32_t motion_blur = graph.AddPass(
      "MotionBlur", render_pass_,
      [this, pool, renderer, pipeline, push_constants](VkCommandBuffer) {
        auto& s = renderer->options();
        push_constants->strength = s.motion_blur_strength;
        push_constants->num_samples = s.motion_blur_samples;
        pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            pipeline, {input_desc_, pool->GetDescriptor("GlobalDescriptor")});
      });
  graph.PassReadsTexture(motion_blur, pipeline_input);
  graph.PassReadsTexture(motion_blur, geo_world_pos);
  graph.PassWritesColor(motion_blur, motion_blur_out);
  graph.SetPassViewport(motion_blur, ctx.viewport_size);
  graph.SetPassClearColor(motion_blur, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      motion_blur,
      [this, renderer, pool, motion_blur, motion_blur_out, pipeline_input,
       geo_world_pos, rw, rh](RenderGraph& g) {
        auto output = g.GetTexture(motion_blur_out);
        auto input = g.GetTexture(pipeline_input);
        auto world_pos = g.GetTexture(geo_world_pos);
        auto linear = renderer->GetDefaultLinearSampler();
        auto nearest = renderer->GetDefaultNearestSampler();
        auto two_input_layout =
            renderer->GetDescriptorLayout("Postprocess2Input");
        auto present_layout = renderer->GetDescriptorLayout("Present");

        if (output_key_ != output.get()) {
          framebuffer_ = render_pass_->CreateFramebuffer(0, {output.get()},
                                                        {rw, rh});
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(present_layout);
          desc->AddCombinedImageSampler(0, output->image_views_[0], linear);
          desc->Bake();
          output_desc_ = desc;
          output_key_ = output.get();
        }
        g.SetPassFramebuffer(motion_blur, framebuffer_);

        if (input_key_ != input.get() || world_pos_key_ != world_pos.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(two_input_layout);
          desc->AddCombinedImageSampler(0, input->image_views_[0], linear);
          desc->AddCombinedImageSampler(1, world_pos->image_views_[0], nearest);
          desc->Bake();
          input_desc_ = desc;
          input_key_ = input.get();
          world_pos_key_ = world_pos.get();
        }

        pool->SetTexture("PipelineOutput", output);
        pool->SetDescriptor("PipelineOutputDescriptor", output_desc_);
      });

  registry.Register("PipelineOutput", motion_blur_out);
}

}  // namespace wiesel
