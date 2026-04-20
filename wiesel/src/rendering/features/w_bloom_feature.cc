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
#include "scene/w_scene.h"
#include "w_engine.h"

namespace wiesel {

BloomFeature::BloomFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
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
  extract_pipeline_->AddColorAttachment(renderer_->GetSwapChainImageFormat());
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
    pipeline->AddColorAttachment(renderer_->GetSwapChainImageFormat());
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
  composite_pipeline_->AddColorAttachment(renderer_->GetSwapChainImageFormat());
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

void BloomFeature::SetupResources(RenderContext& /*ctx*/) {}

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

  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  glm::vec2 half_vp = {rw / 2, rh / 2};

  RGTextureDesc half_desc{
      .name = {},
      .width = rw / 2,
      .height = rh / 2,
      .format = renderer->GetSwapChainImageFormat(),
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true};

  half_desc.name = "bloom.extract";
  RGResource extract_out = graph.DeclareTransient(half_desc);
  half_desc.name = "bloom.blur_h";
  RGResource blur_h_out = graph.DeclareTransient(half_desc);
  half_desc.name = "bloom.blur_v";
  RGResource blur_v_out = graph.DeclareTransient(half_desc);
  RGResource composite_out = graph.DeclareTransient(RGTextureDesc{
      .name = "bloom.composite",
      .width = rw,
      .height = rh,
      .format = renderer->GetSwapChainImageFormat(),
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true});

  RGResource pipeline_input = registry.Get("PipelineOutput");

  // Wire a single-sampler stage (extract or blur iteration): pass reads
  // input_resource, writes output_resource, resolve fn rebuilds the
  // input descriptor when pool assignments change.
  auto emit_stage = [&, this, renderer](
                        const std::shared_ptr<Pipeline>& pipeline,
                        StageBindings& bindings, RGResource input_resource,
                        RGResource output_resource, const char* pass_name) {
    uint32_t pass = graph.AddPass(
        pass_name,
        [renderer, pipeline, &bindings](VkCommandBuffer) {
          pipeline->Bind();
          renderer->DrawFullscreen(pipeline, {bindings.input_desc});
        });
    graph.PassReadsTexture(pass, input_resource);
    graph.PassWritesColor(pass, output_resource);
    graph.SetPassViewport(pass, half_vp);
    graph.SetPassClearColor(pass, {0, 0, 0, 0});
    graph.SetPassResolveFn(
        pass,
        [this, renderer, &bindings, input_resource,
         output_resource](RenderGraph& g) {
          auto output = g.GetTexture(output_resource);
          auto input = g.GetTexture(input_resource);
          auto linear = renderer->GetDefaultLinearSampler();
          auto present_layout = renderer->GetDescriptorLayout("Present");

          if (bindings.output_key != output.get()) {
            bindings.output_key = output.get();
          }

          if (bindings.input_key != input.get()) {
            auto desc = std::make_shared<DescriptorSet>();
            desc->SetLayout(present_layout);
            desc->AddCombinedImageSampler(0, input->image_views_[0], linear);
            desc->Bake();
            bindings.input_desc = desc;
            bindings.input_key = input.get();
          }
        });
  };

  emit_stage(extract_pipeline_, extract_bindings_, pipeline_input, extract_out,
             "Bloom Extract");
  emit_stage(blur_h_pipeline_, blur_h_bindings_, extract_out, blur_h_out,
             "Bloom Blur H");
  emit_stage(blur_v_pipeline_, blur_v_bindings_, blur_h_out, blur_v_out,
             "Bloom Blur V");

  RGResource final_blur = blur_v_out;
  if (hq) {
    half_desc.name = "bloom.blur_h2";
    RGResource blur_h2_out = graph.DeclareTransient(half_desc);
    half_desc.name = "bloom.blur_v2";
    RGResource blur_v2_out = graph.DeclareTransient(half_desc);

    emit_stage(blur_h2_pipeline_, blur_h2_bindings_, blur_v_out, blur_h2_out,
               "Bloom Blur H2");
    emit_stage(blur_v2_pipeline_, blur_v2_bindings_, blur_h2_out, blur_v2_out,
               "Bloom Blur V2");

    final_blur = blur_v2_out;
  }

  // Composite: reads (PipelineOutput, final_blur), writes bloom.composite,
  // publishes itself as the new PipelineOutput for downstream features.
  auto composite_pipeline = composite_pipeline_;
  uint32_t comp_pass = graph.AddPass(
      "Bloom Composite",
      [this, renderer, composite_pipeline](VkCommandBuffer) {
        composite_pipeline->Bind();
        renderer->DrawFullscreen(composite_pipeline, {composite_input_desc_});
      });
  graph.PassReadsTexture(comp_pass, pipeline_input);
  graph.PassReadsTexture(comp_pass, final_blur);
  graph.PassWritesColor(comp_pass, composite_out);
  graph.SetPassViewport(comp_pass, ctx.viewport_size);
  graph.SetPassClearColor(comp_pass, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      comp_pass,
      [this, renderer, pool, composite_out, pipeline_input,
       final_blur](RenderGraph& g) {
        auto output = g.GetTexture(composite_out);
        auto input = g.GetTexture(pipeline_input);
        auto blur = g.GetTexture(final_blur);
        auto linear = renderer->GetDefaultLinearSampler();
        auto present_layout = renderer->GetDescriptorLayout("Present");
        auto two_input_layout =
            renderer->GetDescriptorLayout("Postprocess2Input");

        if (composite_output_key_ != output.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(present_layout);
          desc->AddCombinedImageSampler(0, output->image_views_[0], linear);
          desc->Bake();
          composite_output_desc_ = desc;
          composite_output_key_ = output.get();
        }

        if (composite_input_key_ != input.get() ||
            composite_blur_key_ != blur.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(two_input_layout);
          desc->AddCombinedImageSampler(0, input->image_views_[0], linear);
          desc->AddCombinedImageSampler(1, blur->image_views_[0], linear);
          desc->Bake();
          composite_input_desc_ = desc;
          composite_input_key_ = input.get();
          composite_blur_key_ = blur.get();
        }

        pool->SetTexture("PipelineOutput", output);
        pool->SetDescriptor("PipelineOutputDescriptor", composite_output_desc_);
      });

  registry.Register("PipelineOutput", composite_out);
}

}  // namespace wiesel
