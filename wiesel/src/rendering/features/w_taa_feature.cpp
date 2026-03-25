
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_taa_feature.hpp"
#include "rendering/w_pipeline.hpp"
#include "rendering/w_renderer.hpp"
#include "rendering/w_renderpass.hpp"
#include "scene/w_scene.hpp"

namespace Wiesel {

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
       "/engine/shaders/fullscreen_shader.vert"});

  // TAA pipeline (3 inputs: current frame, history, depth)
  auto taa_frag =
      renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                               ShaderSourceSource, "/engine/shaders/taa.frag"});
  taa_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  taa_pipeline_->SetRenderPass(render_pass_);
  taa_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("TAA"));
  taa_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Global"));
  taa_pipeline_->AddShader(fullscreen_vert);
  taa_pipeline_->AddShader(taa_frag);
  taa_pipeline_->Bake();

  // Copy pipeline (passthrough for history copy)
  auto copy_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/quad_shader.frag"});
  copy_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  copy_pipeline_->SetRenderPass(render_pass_);
  copy_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Present"));
  copy_pipeline_->AddShader(fullscreen_vert);
  copy_pipeline_->AddShader(copy_frag);
  copy_pipeline_->Bake();
}

bool TAAFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.renderer.options().aa_mode == AntiAliasingMode::TAA;
}

void TAAFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("TAAFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  // Textures
  pool.SetTexture("taa.output", renderer.CreateAttachmentTexture(
                                    {rw, rh, AttachmentTextureType::Offscreen,
                                     1, renderer.GetSwapChainImageFormat(),
                                     SamplingMode::DISABLED, true}));

  // TAA history persists across frames; only create if not already present
  if (!pool.HasTexture("taa.history")) {
    pool.SetTexture("taa.history",
                    renderer.CreateAttachmentTexture(
                        {rw, rh, AttachmentTextureType::Offscreen, 1,
                         renderer.GetSwapChainImageFormat(),
                         SamplingMode::DISABLED, true}));
  }

  // Framebuffers
  {
    std::array<AttachmentTexture*, 1> att{pool.GetTexture("taa.output").get()};
    pool.SetFramebuffer("taa",
                        render_pass_->CreateFramebuffer(0, att, {rw, rh}));
  }
  {
    std::array<AttachmentTexture*, 1> att{pool.GetTexture("taa.history").get()};
    pool.SetFramebuffer("taa.history",
                        render_pass_->CreateFramebuffer(0, att, {rw, rh}));
  }

  // Descriptors

  // TAA input: reads PipelineOutput + history + geometry depth (3 inputs)
  auto taa_input_desc = std::make_shared<DescriptorSet>();
  taa_input_desc->SetLayout(renderer.GetDescriptorLayout("TAA"));
  taa_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  taa_input_desc->AddCombinedImageSampler(
      1, pool.GetTexture("taa.history")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  taa_input_desc->AddCombinedImageSampler(
      2, pool.GetTexture("geometry.depth_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  taa_input_desc->Bake();
  pool.SetDescriptor("taa.input", taa_input_desc);

  // TAA copy input: reads taa.output for copying into history
  auto taa_copy_input_desc = std::make_shared<DescriptorSet>();
  taa_copy_input_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  taa_copy_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("taa.output")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  taa_copy_input_desc->Bake();
  pool.SetDescriptor("taa.copy_input", taa_copy_input_desc);

  // TAA output descriptor: reads taa.output
  auto taa_output_desc = std::make_shared<DescriptorSet>();
  taa_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  taa_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("taa.output")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  taa_output_desc->Bake();
  pool.SetDescriptor("taa.output", taa_output_desc);

  // Update pipeline output for the next feature in the chain
  pool.SetTexture("PipelineOutput", pool.GetTexture("taa.output"));
  pool.SetDescriptor("PipelineOutputDescriptor", taa_output_desc);
}

void TAAFeature::AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                           RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("TAAFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;

  // Import TAA textures from pool
  RGResource taa_out =
      graph.ImportTexture("TAAOutput", pool->GetTexture("taa.output"));
  RGResource taa_history =
      graph.ImportTexture("TAAHistory", pool->GetTexture("taa.history"));

  // Get PipelineOutput and geometry depth from registry
  auto pipeline_input = registry.Get("PipelineOutput");
  auto geo_depth = registry.Get("GeoDepth");

  // TAA pass
  auto taa_pipeline = taa_pipeline_;
  uint32_t taa_pass = graph.AddPass(
      "TAA", render_pass_, [pool, renderer, taa_pipeline](VkCommandBuffer) {
        taa_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(taa_pipeline,
                                 {pool->GetDescriptor("taa.input"),
                                  pool->GetDescriptor("GlobalDescriptor")});
      });

  graph.PassReadsTexture(taa_pass, pipeline_input);
  // History is from the previous frame - use external read to get a barrier
  // without creating a dependency edge (avoids cycle with TAA History Copy).
  graph.PassReadsExternalTexture(taa_pass, taa_history);
  graph.PassReadsTexture(taa_pass, geo_depth);
  graph.PassWritesColor(taa_pass, taa_out);
  graph.SetPassFramebuffer(taa_pass, pool->GetFramebuffer("taa"));
  graph.SetPassViewport(taa_pass, ctx.viewport_size);
  graph.SetPassClearColor(taa_pass, {0, 0, 0, 0});

  // TAA History Copy pass
  auto copy_pipeline = copy_pipeline_;
  uint32_t taa_copy = graph.AddPass(
      "TAA History Copy", render_pass_,
      [pool, renderer, copy_pipeline](VkCommandBuffer) {
        copy_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(copy_pipeline,
                                 {pool->GetDescriptor("taa.copy_input")});
      });

  graph.PassReadsTexture(taa_copy, taa_out);
  graph.PassWritesColor(taa_copy, taa_history);
  graph.SetPassFramebuffer(taa_copy, pool->GetFramebuffer("taa.history"));
  graph.SetPassViewport(taa_copy, ctx.viewport_size);
  graph.SetPassClearColor(taa_copy, {0, 0, 0, 0});

  // Update PipelineOutput for the next feature in the chain
  registry.Register("PipelineOutput", taa_out);
}

}  // namespace Wiesel
