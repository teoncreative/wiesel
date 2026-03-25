
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_bloom_feature.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_scene.h"

namespace Wiesel {

BloomFeature::BloomFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Shared postprocess render pass (1 color, no MSAA)
  render_pass_ = std::make_shared<RenderPass>(PassType::PostProcess,
                                              "PostProcess RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  push_constants_ = std::make_shared<BloomPushConstants>();

  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});

  // Bloom extract pipeline
  auto extract_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/bloom_extract.frag"});
  extract_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  extract_pipeline_->SetRenderPass(render_pass_);
  extract_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Present"));
  extract_pipeline_->AddPushConstant(push_constants_,
                                     VK_SHADER_STAGE_FRAGMENT_BIT);
  extract_pipeline_->AddShader(fullscreen_vert);
  extract_pipeline_->AddShader(extract_frag);
  extract_pipeline_->Bake();

  // Bloom blur H pipeline
  auto blur_h_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/bloom_blur.frag"});
  blur_h_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  blur_h_pipeline_->SetRenderPass(render_pass_);
  blur_h_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Present"));
  blur_h_pipeline_->AddShader(fullscreen_vert);
  blur_h_pipeline_->AddShader(blur_h_frag);
  blur_h_pipeline_->Bake();

  // Bloom blur V pipeline (with BLUR_VERTICAL define)
  auto blur_v_frag =
      renderer_->CreateShader({ShaderTypeFragment,
                               ShaderLangGLSL,
                               "main",
                               ShaderSourceSource,
                               "engine://shaders/bloom_blur.frag",
                               {"BLUR_VERTICAL"}});
  blur_v_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  blur_v_pipeline_->SetRenderPass(render_pass_);
  blur_v_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Present"));
  blur_v_pipeline_->AddShader(fullscreen_vert);
  blur_v_pipeline_->AddShader(blur_v_frag);
  blur_v_pipeline_->Bake();

  // Bloom composite pipeline (2 inputs: scene + blurred bloom)
  auto composite_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/bloom_composite.frag"});
  composite_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  composite_pipeline_->SetRenderPass(render_pass_);
  composite_pipeline_->AddInputLayout(
      renderer_->GetDescriptorLayout("Postprocess2Input"));
  composite_pipeline_->AddPushConstant(push_constants_,
                                       VK_SHADER_STAGE_FRAGMENT_BIT);
  composite_pipeline_->AddShader(fullscreen_vert);
  composite_pipeline_->AddShader(composite_frag);
  composite_pipeline_->Bake();
}

bool BloomFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.renderer.options().bloom_enabled;
}

void BloomFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("BloomFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  uint32_t hrw = rw / 2;
  uint32_t hrh = rh / 2;

  // Textures
  // Half-res textures for extract and blur passes
  pool.SetTexture(
      "bloom.extract",
      renderer.CreateAttachmentTexture(
          {hrw, hrh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));
  pool.SetTexture(
      "bloom.blur_h",
      renderer.CreateAttachmentTexture(
          {hrw, hrh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));
  pool.SetTexture(
      "bloom.blur_v",
      renderer.CreateAttachmentTexture(
          {hrw, hrh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));
  // Full-res composite output
  pool.SetTexture(
      "bloom.composite",
      renderer.CreateAttachmentTexture(
          {rw, rh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));

  // Framebuffers
  pool.SetFramebuffer(
      "bloom.extract",
      render_pass_->CreateFramebuffer(
          0, {pool.GetTexture("bloom.extract").get()}, {hrw, hrh}));
  pool.SetFramebuffer(
      "bloom.blur_h",
      render_pass_->CreateFramebuffer(
          0, {pool.GetTexture("bloom.blur_h").get()}, {hrw, hrh}));
  pool.SetFramebuffer(
      "bloom.blur_v",
      render_pass_->CreateFramebuffer(
          0, {pool.GetTexture("bloom.blur_v").get()}, {hrw, hrh}));
  pool.SetFramebuffer(
      "bloom.composite",
      render_pass_->CreateFramebuffer(
          0, {pool.GetTexture("bloom.composite").get()}, {rw, rh}));

  // Descriptors

  // Bloom extract input: reads PipelineOutput (whatever the previous feature set)
  auto extract_input_desc = std::make_shared<DescriptorSet>();
  extract_input_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  extract_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  extract_input_desc->Bake();
  pool.SetDescriptor("bloom.extract_input", extract_input_desc);

  // Bloom blur H input: reads bloom extract output
  auto blur_h_input_desc = std::make_shared<DescriptorSet>();
  blur_h_input_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  blur_h_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("bloom.extract")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  blur_h_input_desc->Bake();
  pool.SetDescriptor("bloom.blur_h_input", blur_h_input_desc);

  // Bloom blur V input: reads bloom blur H output
  auto blur_v_input_desc = std::make_shared<DescriptorSet>();
  blur_v_input_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  blur_v_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("bloom.blur_h")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  blur_v_input_desc->Bake();
  pool.SetDescriptor("bloom.blur_v_input", blur_v_input_desc);

  // Bloom composite input: reads PipelineOutput + bloom blur V (2 inputs)
  auto composite_input_desc = std::make_shared<DescriptorSet>();
  composite_input_desc->SetLayout(
      renderer.GetDescriptorLayout("Postprocess2Input"));
  composite_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  composite_input_desc->AddCombinedImageSampler(
      1, pool.GetTexture("bloom.blur_v")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  composite_input_desc->Bake();
  pool.SetDescriptor("bloom.composite_input", composite_input_desc);

  // Bloom output descriptor: reads bloom composite result
  auto bloom_output_desc = std::make_shared<DescriptorSet>();
  bloom_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  bloom_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("bloom.composite")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  bloom_output_desc->Bake();
  pool.SetDescriptor("bloom.output", bloom_output_desc);

  // Update pipeline output for the next feature in the chain
  pool.SetTexture("PipelineOutput", pool.GetTexture("bloom.composite"));
  pool.SetDescriptor("PipelineOutputDescriptor", bloom_output_desc);
}

void BloomFeature::AddPasses(RenderGraph& graph,
                             RenderResourceRegistry& registry,
                             RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("BloomFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto push_constants = push_constants_;

  // Import bloom textures from pool
  RGResource bloom_extract_out =
      graph.ImportTexture("BloomExtract", pool->GetTexture("bloom.extract"));
  RGResource bloom_blur_h_out =
      graph.ImportTexture("BloomBlurH", pool->GetTexture("bloom.blur_h"));
  RGResource bloom_blur_v_out =
      graph.ImportTexture("BloomBlurV", pool->GetTexture("bloom.blur_v"));
  RGResource bloom_composite_out = graph.ImportTexture(
      "BloomComposite", pool->GetTexture("bloom.composite"));

  // Get PipelineOutput from registry (set by previous feature)
  auto pipeline_input = registry.Get("PipelineOutput");

  // Bloom Extract pass (half-res)
  auto extract_pipeline = extract_pipeline_;
  uint32_t bloom_extract = graph.AddPass(
      "Bloom Extract", render_pass_,
      [pool, renderer, extract_pipeline, push_constants](VkCommandBuffer) {
        auto& s = renderer->options();
        push_constants->threshold = s.bloom_threshold;
        push_constants->intensity = s.bloom_intensity;
        extract_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(extract_pipeline,
                                 {pool->GetDescriptor("bloom.extract_input")});
      });
  graph.PassReadsTexture(bloom_extract, pipeline_input);
  graph.PassWritesColor(bloom_extract, bloom_extract_out);
  graph.SetPassFramebuffer(bloom_extract,
                           pool->GetFramebuffer("bloom.extract"));
  graph.SetPassViewport(bloom_extract,
                        {ctx.viewport_size.x / 2, ctx.viewport_size.y / 2});
  graph.SetPassClearColor(bloom_extract, {0, 0, 0, 0});

  // Bloom Blur Horizontal pass (half-res)
  auto blur_h_pipeline = blur_h_pipeline_;
  uint32_t bloom_blur_h = graph.AddPass(
      "Bloom Blur H", render_pass_,
      [pool, renderer, blur_h_pipeline](VkCommandBuffer) {
        blur_h_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(blur_h_pipeline,
                                 {pool->GetDescriptor("bloom.blur_h_input")});
      });
  graph.PassReadsTexture(bloom_blur_h, bloom_extract_out);
  graph.PassWritesColor(bloom_blur_h, bloom_blur_h_out);
  graph.SetPassFramebuffer(bloom_blur_h, pool->GetFramebuffer("bloom.blur_h"));
  graph.SetPassViewport(bloom_blur_h,
                        {ctx.viewport_size.x / 2, ctx.viewport_size.y / 2});
  graph.SetPassClearColor(bloom_blur_h, {0, 0, 0, 0});

  // Bloom Blur Vertical pass (half-res)
  auto blur_v_pipeline = blur_v_pipeline_;
  uint32_t bloom_blur_v = graph.AddPass(
      "Bloom Blur V", render_pass_,
      [pool, renderer, blur_v_pipeline](VkCommandBuffer) {
        blur_v_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(blur_v_pipeline,
                                 {pool->GetDescriptor("bloom.blur_v_input")});
      });
  graph.PassReadsTexture(bloom_blur_v, bloom_blur_h_out);
  graph.PassWritesColor(bloom_blur_v, bloom_blur_v_out);
  graph.SetPassFramebuffer(bloom_blur_v, pool->GetFramebuffer("bloom.blur_v"));
  graph.SetPassViewport(bloom_blur_v,
                        {ctx.viewport_size.x / 2, ctx.viewport_size.y / 2});
  graph.SetPassClearColor(bloom_blur_v, {0, 0, 0, 0});

  // Bloom Composite pass (full-res)
  auto composite_pipeline = composite_pipeline_;
  uint32_t bloom_comp = graph.AddPass(
      "Bloom Composite", render_pass_,
      [pool, renderer, composite_pipeline, push_constants](VkCommandBuffer) {
        auto& s = renderer->options();
        push_constants->threshold = s.bloom_threshold;
        push_constants->intensity = s.bloom_intensity;
        composite_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            composite_pipeline, {pool->GetDescriptor("bloom.composite_input")});
      });
  graph.PassReadsTexture(bloom_comp, pipeline_input);
  graph.PassReadsTexture(bloom_comp, bloom_blur_v_out);
  graph.PassWritesColor(bloom_comp, bloom_composite_out);
  graph.SetPassFramebuffer(bloom_comp, pool->GetFramebuffer("bloom.composite"));
  graph.SetPassViewport(bloom_comp, ctx.viewport_size);
  graph.SetPassClearColor(bloom_comp, {0, 0, 0, 0});

  // Update PipelineOutput for the next feature in the chain
  registry.Register("PipelineOutput", bloom_composite_out);
}

}  // namespace Wiesel
