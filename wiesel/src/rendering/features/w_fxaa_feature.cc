
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
#include "rendering/w_framebuffer.h"
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

void FXAAFeature::SetupResources(RenderContext& /*ctx*/) {
  // Transient output + per-frame-resolved bindings. Nothing persistent.
}

void FXAAFeature::AddPasses(RenderGraph& graph,
                            RenderResourceRegistry& registry,
                            RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("FXAAFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto push_constants = push_constants_;
  glm::vec2 viewport_size = ctx.viewport_size;
  uint32_t rw = static_cast<uint32_t>(viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(viewport_size.y);

  RGResource fxaa_out = graph.DeclareTransient(RGTextureDesc{
      .name = "fxaa.color",
      .width = rw,
      .height = rh,
      .format = renderer->GetSwapChainImageFormat(),
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true});

  RGResource pipeline_input = registry.Get("PipelineOutput");

  auto pipeline = pipeline_;
  uint32_t fxaa_pass = graph.AddPass(
      "FXAA", render_pass_,
      [this, renderer, pipeline, push_constants,
       viewport_size](VkCommandBuffer) {
        push_constants->inverse_screen_size = {1.0f / viewport_size.x,
                                               1.0f / viewport_size.y};
        pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(pipeline, {input_desc_});
      });
  graph.PassReadsTexture(fxaa_pass, pipeline_input);
  graph.PassWritesColor(fxaa_pass, fxaa_out);
  graph.SetPassViewport(fxaa_pass, viewport_size);
  graph.SetPassClearColor(fxaa_pass, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      fxaa_pass,
      [this, renderer, pool, fxaa_pass, fxaa_out, pipeline_input, rw,
       rh](RenderGraph& g) {
        auto output = g.GetTexture(fxaa_out);
        auto input = g.GetTexture(pipeline_input);
        auto present_layout = renderer->GetDescriptorLayout("Present");
        auto linear = renderer->GetDefaultLinearSampler();

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
        g.SetPassFramebuffer(fxaa_pass, framebuffer_);

        if (input_key_ != input.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(present_layout);
          desc->AddCombinedImageSampler(0, input->image_views_[0], linear);
          desc->Bake();
          input_desc_ = desc;
          input_key_ = input.get();
        }

        pool->SetTexture("PipelineOutput", output);
        pool->SetDescriptor("PipelineOutputDescriptor", output_desc_);
      });

  registry.Register("PipelineOutput", fxaa_out);
}

}  // namespace wiesel
