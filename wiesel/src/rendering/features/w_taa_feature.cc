
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_taa_feature.h"
#include "rendering/w_framebuffer.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_scene.h"

namespace wiesel {

TAAFeature::TAAFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Postprocess render pass (1 color, no MSAA)
  render_pass_ = std::make_shared<RenderPass>(PassType::PostProcess,
                                              "PostProcess RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});

  // TAA pipeline (3 inputs: current frame, history, depth)
  auto taa_frag = renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL,
                                           "main", ShaderSourceSource,
                                           "engine://shaders/taa.frag"});
  taa_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, false, false, false});
  taa_pipeline_->SetRenderPass(render_pass_);
  taa_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("TAA"));
  taa_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Global"));
  taa_pipeline_->AddShader(fullscreen_vert);
  taa_pipeline_->AddShader(taa_frag);
  taa_pipeline_->Bake();

  // Copy pipeline (passthrough for history copy)
  auto copy_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/quad_shader.frag"});
  copy_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, false, false, false});
  copy_pipeline_->SetRenderPass(render_pass_);
  copy_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Present"));
  copy_pipeline_->AddShader(fullscreen_vert);
  copy_pipeline_->AddShader(copy_frag);
  copy_pipeline_->Bake();
}

bool TAAFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.renderer.options().aa_mode == AntiAliasingMode::TAA;
}

void TAAFeature::SetupResources(RenderContext& /*ctx*/) {}

void TAAFeature::AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                           RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("TAAFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  // Lazily (re)create the persistent history buffer when the viewport
  // changes. Everything else is transient.
  if (!history_texture_ || history_width_ != rw || history_height_ != rh) {
    AttachmentTextureProps props{};
    props.width = rw;
    props.height = rh;
    props.type = AttachmentTextureType::Offscreen;
    props.image_count = 1;
    props.image_format = renderer->GetSwapChainImageFormat();
    props.sampling_mode = SamplingMode::DISABLED;
    props.sampled = true;
    history_texture_ = renderer->CreateAttachmentTexture(props);
    history_width_ = rw;
    history_height_ = rh;
    // Invalidate cached bindings that reference the old history.
    taa_history_key_ = nullptr;
    copy_history_key_ = nullptr;
  }

  RGResource taa_out = graph.DeclareTransient(RGTextureDesc{
      .name = "taa.output",
      .width = rw,
      .height = rh,
      .format = renderer->GetSwapChainImageFormat(),
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true});
  RGResource taa_history =
      graph.ImportTexture("TAAHistory", history_texture_);

  RGResource pipeline_input = registry.Get("PipelineOutput");
  RGResource geo_depth = registry.Get("GeoDepth");

  // --- TAA pass ---
  auto taa_pipeline = taa_pipeline_;
  uint32_t taa_pass = graph.AddPass(
      "TAA", render_pass_,
      [this, pool, renderer, taa_pipeline](VkCommandBuffer) {
        taa_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            taa_pipeline,
            {taa_input_desc_, pool->GetDescriptor("GlobalDescriptor")});
      });
  graph.PassReadsTexture(taa_pass, pipeline_input);
  // History is from the previous frame - external read to barrier without
  // creating an edge (avoids cycle with TAA History Copy).
  graph.PassReadsExternalTexture(taa_pass, taa_history);
  graph.PassReadsTexture(taa_pass, geo_depth);
  graph.PassWritesColor(taa_pass, taa_out);
  graph.SetPassViewport(taa_pass, ctx.viewport_size);
  graph.SetPassClearColor(taa_pass, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      taa_pass,
      [this, renderer, pool, taa_pass, taa_out, taa_history, pipeline_input,
       geo_depth, rw, rh](RenderGraph& g) {
        auto output = g.GetTexture(taa_out);
        auto input = g.GetTexture(pipeline_input);
        auto history = g.GetTexture(taa_history);
        auto depth = g.GetTexture(geo_depth);
        auto linear = renderer->GetDefaultLinearSampler();
        auto nearest = renderer->GetDefaultNearestSampler();
        auto taa_layout = renderer->GetDescriptorLayout("TAA");
        auto present_layout = renderer->GetDescriptorLayout("Present");

        if (taa_output_key_ != output.get()) {
          taa_framebuffer_ = render_pass_->CreateFramebuffer(0, {output.get()},
                                                             {rw, rh});
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(present_layout);
          desc->AddCombinedImageSampler(0, output->image_views_[0], linear);
          desc->Bake();
          output_desc_ = desc;
          taa_output_key_ = output.get();
        }
        g.SetPassFramebuffer(taa_pass, taa_framebuffer_);

        if (taa_input_key_ != input.get() ||
            taa_history_key_ != history.get() ||
            taa_depth_key_ != depth.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(taa_layout);
          desc->AddCombinedImageSampler(0, input->image_views_[0], linear);
          desc->AddCombinedImageSampler(1, history->image_views_[0], linear);
          desc->AddCombinedImageSampler(2, depth->image_views_[0], nearest);
          desc->Bake();
          taa_input_desc_ = desc;
          taa_input_key_ = input.get();
          taa_history_key_ = history.get();
          taa_depth_key_ = depth.get();
        }

        pool->SetTexture("PipelineOutput", output);
        pool->SetDescriptor("PipelineOutputDescriptor", output_desc_);
      });

  // --- History copy pass ---
  auto copy_pipeline = copy_pipeline_;
  uint32_t taa_copy = graph.AddPass(
      "TAA History Copy", render_pass_,
      [this, renderer, copy_pipeline](VkCommandBuffer) {
        copy_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(copy_pipeline, {copy_input_desc_});
      });
  graph.PassReadsTexture(taa_copy, taa_out);
  graph.PassWritesColor(taa_copy, taa_history);
  graph.SetPassViewport(taa_copy, ctx.viewport_size);
  graph.SetPassClearColor(taa_copy, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      taa_copy,
      [this, renderer, taa_copy, taa_out, taa_history, rw, rh](RenderGraph& g) {
        auto history = g.GetTexture(taa_history);
        auto taa_output = g.GetTexture(taa_out);
        auto linear = renderer->GetDefaultLinearSampler();
        auto present_layout = renderer->GetDescriptorLayout("Present");

        if (copy_history_key_ != history.get()) {
          copy_framebuffer_ = render_pass_->CreateFramebuffer(
              0, {history.get()}, {rw, rh});
          copy_history_key_ = history.get();
        }
        g.SetPassFramebuffer(taa_copy, copy_framebuffer_);

        if (copy_input_key_ != taa_output.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(present_layout);
          desc->AddCombinedImageSampler(0, taa_output->image_views_[0], linear);
          desc->Bake();
          copy_input_desc_ = desc;
          copy_input_key_ = taa_output.get();
        }
      });

  registry.Register("PipelineOutput", taa_out);
}

}  // namespace wiesel
