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
#include "w_engine.h"

namespace wiesel {

BloomFeature::BloomFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  render_pass_ = std::make_shared<RenderPass>(PassType::PostProcess,
                                              "PostProcess RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  extract_push_ = std::make_shared<BloomExtractPushConstants>();
  blur_push_ = std::make_shared<BloomBlurPushConstants>();
  composite_push_ = std::make_shared<BloomCompositePushConstants>();

  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  auto present_layout = renderer_->GetDescriptorLayout("Present");

  // Extract pipeline
  auto extract_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/bloom_extract.frag"});
  {
    PipelineProperties props{};
    props.enable_depth_test = false;
    props.enable_depth_write = false;
    extract_pipeline_ = std::make_shared<Pipeline>(props);
  }
  extract_pipeline_->SetRenderPass(render_pass_);
  extract_pipeline_->AddInputLayout(present_layout);
  extract_pipeline_->AddPushConstant(extract_push_,
                                     VK_SHADER_STAGE_FRAGMENT_BIT);
  extract_pipeline_->AddShader(fullscreen_vert);
  extract_pipeline_->AddShader(extract_frag);
  extract_pipeline_->Bake();

  // Blur pipelines using shared gaussian_blur.frag
  auto blur_h_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
                               "engine://shaders/gaussian_blur.frag",
                               {"USE_PUSH_RADIUS"}});
  auto blur_v_frag =
      renderer_->CreateShader({ShaderTypeFragment,
                               ShaderLangGLSL,
                               "main",
                               ShaderSourceSource,
                               "engine://shaders/gaussian_blur.frag",
                               {"USE_PUSH_RADIUS", "BLUR_VERTICAL"}});

  auto create_blur_pipeline = [&](const std::shared_ptr<Shader>& frag) {
    PipelineProperties props{};
    props.enable_depth_test = false;
    props.enable_depth_write = false;
    auto pipeline = std::make_shared<Pipeline>(props);
    pipeline->SetRenderPass(render_pass_);
    pipeline->AddInputLayout(present_layout);
    pipeline->AddPushConstant(blur_push_, VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline->AddShader(fullscreen_vert);
    pipeline->AddShader(frag);
    pipeline->Bake();
    return pipeline;
  };

  blur_h_pipeline_ = create_blur_pipeline(blur_h_frag);
  blur_v_pipeline_ = create_blur_pipeline(blur_v_frag);
  blur_h2_pipeline_ = create_blur_pipeline(blur_h_frag);
  blur_v2_pipeline_ = create_blur_pipeline(blur_v_frag);

  // Composite pipeline
  auto composite_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/bloom_composite.frag"});
  {
    PipelineProperties props{};
    props.enable_depth_test = false;
    props.enable_depth_write = false;
    composite_pipeline_ = std::make_shared<Pipeline>(props);
  }
  composite_pipeline_->SetRenderPass(render_pass_);
  composite_pipeline_->AddInputLayout(
      renderer_->GetDescriptorLayout("Postprocess2Input"));
  composite_pipeline_->AddPushConstant(composite_push_,
                                       VK_SHADER_STAGE_FRAGMENT_BIT);
  composite_pipeline_->AddShader(fullscreen_vert);
  composite_pipeline_->AddShader(composite_frag);
  composite_pipeline_->Bake();
}

bool BloomFeature::IsEnabled(const RenderContext&) const {
  return true;
}

void BloomFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("BloomFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  uint32_t hrw = rw / 2;
  uint32_t hrh = rh / 2;
  auto linear = renderer.GetDefaultLinearSampler();
  auto present_layout = renderer.GetDescriptorLayout("Present");

  auto make_half = [&](const std::string& name) {
    pool.SetTexture(name, renderer.CreateAttachmentTexture(
                              {hrw, hrh, AttachmentTextureType::Offscreen, 1,
                               renderer.GetSwapChainImageFormat(),
                               SamplingMode::DISABLED, true}));
  };

  make_half("bloom.extract");
  make_half("bloom.blur_h");
  make_half("bloom.blur_v");
  make_half("bloom.blur_h2");
  make_half("bloom.blur_v2");

  pool.SetTexture(
      "bloom.composite",
      renderer.CreateAttachmentTexture(
          {rw, rh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));

  auto make_fb = [&](const std::string& name, uint32_t w, uint32_t h) {
    pool.SetFramebuffer(
        name, render_pass_->CreateFramebuffer(
                  0, {pool.GetTexture(name).get()}, {w, h}));
  };

  make_fb("bloom.extract", hrw, hrh);
  make_fb("bloom.blur_h", hrw, hrh);
  make_fb("bloom.blur_v", hrw, hrh);
  make_fb("bloom.blur_h2", hrw, hrh);
  make_fb("bloom.blur_v2", hrw, hrh);
  make_fb("bloom.composite", rw, rh);

  auto make_desc = [&](const std::string& name,
                       const std::string& texture_name) {
    auto desc = std::make_shared<DescriptorSet>();
    desc->SetLayout(present_layout);
    desc->AddCombinedImageSampler(
        0, pool.GetTexture(texture_name)->image_views_[0], linear);
    desc->Bake();
    pool.SetDescriptor(name, desc);
  };

  make_desc("bloom.extract_input", "PipelineOutput");
  make_desc("bloom.blur_h_input", "bloom.extract");
  make_desc("bloom.blur_v_input", "bloom.blur_h");
  make_desc("bloom.blur_h2_input", "bloom.blur_v");
  make_desc("bloom.blur_v2_input", "bloom.blur_h2");

  auto composite_input = std::make_shared<DescriptorSet>();
  composite_input->SetLayout(
      renderer.GetDescriptorLayout("Postprocess2Input"));
  composite_input->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0], linear);
  composite_input->AddCombinedImageSampler(
      1, pool.GetTexture("bloom.blur_v")->image_views_[0], linear);
  composite_input->Bake();
  pool.SetDescriptor("bloom.composite_input", composite_input);

  // HQ composite reads from blur_v2 instead
  auto composite_hq_input = std::make_shared<DescriptorSet>();
  composite_hq_input->SetLayout(
      renderer.GetDescriptorLayout("Postprocess2Input"));
  composite_hq_input->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0], linear);
  composite_hq_input->AddCombinedImageSampler(
      1, pool.GetTexture("bloom.blur_v2")->image_views_[0], linear);
  composite_hq_input->Bake();
  pool.SetDescriptor("bloom.composite_hq_input", composite_hq_input);

  auto output_desc = std::make_shared<DescriptorSet>();
  output_desc->SetLayout(present_layout);
  output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("bloom.composite")->image_views_[0], linear);
  output_desc->Bake();
  pool.SetDescriptor("bloom.output", output_desc);

  pool.SetTexture("PipelineOutput", pool.GetTexture("bloom.composite"));
  pool.SetDescriptor("PipelineOutputDescriptor", output_desc);
}

void BloomFeature::AddPasses(RenderGraph& graph,
                             RenderResourceRegistry& registry,
                             RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("BloomFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto& opts = renderer_->options();
  bool hq = opts.bloom_high_quality;

  extract_push_->threshold = opts.bloom_threshold;
  extract_push_->clamp_value = opts.bloom_clamp;

  float base_radius = 8.0f;
  blur_push_->radius = base_radius * static_cast<float>(opts.bloom_scatter);

  glm::vec3 tint = opts.bloom_tint;
  composite_push_->tint_intensity =
      glm::vec4(tint, static_cast<float>(opts.bloom_intensity));

  glm::vec2 half_vp = {ctx.viewport_size.x / 2, ctx.viewport_size.y / 2};

  RGResource pipeline_input = registry.Get("PipelineOutput");
  RGResource extract_out =
      graph.ImportTexture("BloomExtract", pool->GetTexture("bloom.extract"));
  RGResource blur_h_out =
      graph.ImportTexture("BloomBlurH", pool->GetTexture("bloom.blur_h"));
  RGResource blur_v_out =
      graph.ImportTexture("BloomBlurV", pool->GetTexture("bloom.blur_v"));
  RGResource composite_out = graph.ImportTexture(
      "BloomComposite", pool->GetTexture("bloom.composite"));

  // Extract
  auto extract_pipeline = extract_pipeline_;
  uint32_t extract_pass = graph.AddPass(
      "Bloom Extract", render_pass_,
      [pool, extract_pipeline](VkCommandBuffer) {
        extract_pipeline->Bind(PipelineBindPointGraphics);
        Engine::renderer()->DrawFullscreen(
            extract_pipeline, {pool->GetDescriptor("bloom.extract_input")});
      });
  graph.PassReadsTexture(extract_pass, pipeline_input);
  graph.PassWritesColor(extract_pass, extract_out);
  graph.SetPassFramebuffer(extract_pass, pool->GetFramebuffer("bloom.extract"));
  graph.SetPassViewport(extract_pass, half_vp);
  graph.SetPassClearColor(extract_pass, {0, 0, 0, 0});

  // Blur H
  auto blur_h_pipeline = blur_h_pipeline_;
  uint32_t blur_h_pass = graph.AddPass(
      "Bloom Blur H", render_pass_,
      [pool, blur_h_pipeline](VkCommandBuffer) {
        blur_h_pipeline->Bind(PipelineBindPointGraphics);
        Engine::renderer()->DrawFullscreen(
            blur_h_pipeline, {pool->GetDescriptor("bloom.blur_h_input")});
      });
  graph.PassReadsTexture(blur_h_pass, extract_out);
  graph.PassWritesColor(blur_h_pass, blur_h_out);
  graph.SetPassFramebuffer(blur_h_pass, pool->GetFramebuffer("bloom.blur_h"));
  graph.SetPassViewport(blur_h_pass, half_vp);
  graph.SetPassClearColor(blur_h_pass, {0, 0, 0, 0});

  // Blur V
  auto blur_v_pipeline = blur_v_pipeline_;
  uint32_t blur_v_pass = graph.AddPass(
      "Bloom Blur V", render_pass_,
      [pool, blur_v_pipeline](VkCommandBuffer) {
        blur_v_pipeline->Bind(PipelineBindPointGraphics);
        Engine::renderer()->DrawFullscreen(
            blur_v_pipeline, {pool->GetDescriptor("bloom.blur_v_input")});
      });
  graph.PassReadsTexture(blur_v_pass, blur_h_out);
  graph.PassWritesColor(blur_v_pass, blur_v_out);
  graph.SetPassFramebuffer(blur_v_pass, pool->GetFramebuffer("bloom.blur_v"));
  graph.SetPassViewport(blur_v_pass, half_vp);
  graph.SetPassClearColor(blur_v_pass, {0, 0, 0, 0});

  // HQ: extra blur iterations
  RGResource final_blur = blur_v_out;
  if (hq) {
    RGResource blur_h2_out =
        graph.ImportTexture("BloomBlurH2", pool->GetTexture("bloom.blur_h2"));
    RGResource blur_v2_out =
        graph.ImportTexture("BloomBlurV2", pool->GetTexture("bloom.blur_v2"));

    auto blur_h2_pipeline = blur_h2_pipeline_;
    uint32_t blur_h2_pass = graph.AddPass(
        "Bloom Blur H2", render_pass_,
        [pool, blur_h2_pipeline](VkCommandBuffer) {
          blur_h2_pipeline->Bind(PipelineBindPointGraphics);
          Engine::renderer()->DrawFullscreen(
              blur_h2_pipeline,
              {pool->GetDescriptor("bloom.blur_h2_input")});
        });
    graph.PassReadsTexture(blur_h2_pass, blur_v_out);
    graph.PassWritesColor(blur_h2_pass, blur_h2_out);
    graph.SetPassFramebuffer(blur_h2_pass,
                             pool->GetFramebuffer("bloom.blur_h2"));
    graph.SetPassViewport(blur_h2_pass, half_vp);
    graph.SetPassClearColor(blur_h2_pass, {0, 0, 0, 0});

    auto blur_v2_pipeline = blur_v2_pipeline_;
    uint32_t blur_v2_pass = graph.AddPass(
        "Bloom Blur V2", render_pass_,
        [pool, blur_v2_pipeline](VkCommandBuffer) {
          blur_v2_pipeline->Bind(PipelineBindPointGraphics);
          Engine::renderer()->DrawFullscreen(
              blur_v2_pipeline,
              {pool->GetDescriptor("bloom.blur_v2_input")});
        });
    graph.PassReadsTexture(blur_v2_pass, blur_h2_out);
    graph.PassWritesColor(blur_v2_pass, blur_v2_out);
    graph.SetPassFramebuffer(blur_v2_pass,
                             pool->GetFramebuffer("bloom.blur_v2"));
    graph.SetPassViewport(blur_v2_pass, half_vp);
    graph.SetPassClearColor(blur_v2_pass, {0, 0, 0, 0});

    final_blur = blur_v2_out;
  }

  // Composite
  std::string comp_input_name =
      hq ? "bloom.composite_hq_input" : "bloom.composite_input";
  auto composite_pipeline = composite_pipeline_;
  uint32_t comp_pass = graph.AddPass(
      "Bloom Composite", render_pass_,
      [pool, composite_pipeline, comp_input_name](VkCommandBuffer) {
        composite_pipeline->Bind(PipelineBindPointGraphics);
        Engine::renderer()->DrawFullscreen(
            composite_pipeline, {pool->GetDescriptor(comp_input_name)});
      });
  graph.PassReadsTexture(comp_pass, pipeline_input);
  graph.PassReadsTexture(comp_pass, final_blur);
  graph.PassWritesColor(comp_pass, composite_out);
  graph.SetPassFramebuffer(comp_pass, pool->GetFramebuffer("bloom.composite"));
  graph.SetPassViewport(comp_pass, ctx.viewport_size);
  graph.SetPassClearColor(comp_pass, {0, 0, 0, 0});

  registry.Register("PipelineOutput", composite_out);
}

}  // namespace wiesel
