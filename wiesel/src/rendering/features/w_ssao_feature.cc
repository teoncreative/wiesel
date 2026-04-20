
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_ssao_feature.h"
#include "rendering/w_buffer.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "scene/w_scene.h"

namespace wiesel {

SSAOFeature::SSAOFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Pipelines (all fullscreen quad)
  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});

  // Gen pipeline
  auto ssao_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/ssao_gen_shader.frag"});
  gen_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, false, false, false});
  gen_pipeline_->AddColorAttachment(VK_FORMAT_R8_UNORM);
  gen_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("SSAOGen"));
  gen_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Global"));
  gen_pipeline_->AddShader(fullscreen_vert);
  gen_pipeline_->AddShader(ssao_frag);
  gen_pipeline_->Bake();

  // Blur H pipeline
  auto blur_h_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/ssao_blur_shader.frag"});
  blur_horz_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, false, false, false});
  blur_horz_pipeline_->AddColorAttachment(VK_FORMAT_R8_UNORM);
  blur_horz_pipeline_->AddInputLayout(
      renderer_->GetDescriptorLayout("SSAOBlur"));
  blur_horz_pipeline_->AddShader(fullscreen_vert);
  blur_horz_pipeline_->AddShader(blur_h_frag);
  blur_horz_pipeline_->Bake();

  // Blur V pipeline (with BLUR_VERTICAL define)
  auto blur_v_frag =
      renderer_->CreateShader({ShaderTypeFragment,
                               ShaderLangGLSL,
                               "main",
                               ShaderSourceSource,
                               "engine://shaders/ssao_blur_shader.frag",
                               {"BLUR_VERTICAL"}});
  blur_vert_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, false, false, false});
  blur_vert_pipeline_->AddColorAttachment(VK_FORMAT_R8_UNORM);
  blur_vert_pipeline_->AddInputLayout(
      renderer_->GetDescriptorLayout("SSAOBlur"));
  blur_vert_pipeline_->AddShader(fullscreen_vert);
  blur_vert_pipeline_->AddShader(blur_v_frag);
  blur_vert_pipeline_->Bake();
}

bool SSAOFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.renderer.options().ssao_enabled;
}

void SSAOFeature::SetupResources(RenderContext& /*ctx*/) {
}

void SSAOFeature::AddPasses(RenderGraph& graph,
                            RenderResourceRegistry& registry,
                            RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("SSAOFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  // Declare the three SSAO work textures as transient. The pool aliases
  // non-overlapping transients with matching descriptors and reuses them
  // across frames.
  RGResource ssao_out = graph.DeclareTransient(RGTextureDesc{
      .name = "ssao.color",
      .width = rw / 2,
      .height = rh / 2,
      .format = VK_FORMAT_R8_UNORM,
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true});
  RGResource ssao_blur_h = graph.DeclareTransient(RGTextureDesc{
      .name = "ssao.blur_h",
      .width = rw,
      .height = rh,
      .format = VK_FORMAT_R8_UNORM,
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true});
  RGResource ssao_blur_v = graph.DeclareTransient(RGTextureDesc{
      .name = "ssao.blur_v",
      .width = rw,
      .height = rh,
      .format = VK_FORMAT_R8_UNORM,
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true});

  RGResource ssao_noise =
      graph.ImportTexture("SSAONoise", renderer->GetSSAONoise());

  RGResource geo_view_pos = registry.Get("GeoViewPos");
  RGResource geo_normal = registry.Get("GeoNormal");
  RGResource geo_depth = registry.Get("GeoDepth");

  // --- Gen pass ---
  uint32_t ssao_gen = graph.AddPass(
      "SSAO Gen",
      [this, renderer, pool](VkCommandBuffer) {
        gen_pipeline_->Bind();
        renderer->DrawFullscreen(gen_pipeline_,
                                 {gen_bindings_.gen_desc,
                                  pool->GetDescriptor("GlobalDescriptor")});
      });
  graph.PassReadsTexture(ssao_gen, geo_view_pos);
  graph.PassReadsTexture(ssao_gen, geo_normal);
  graph.PassReadsTexture(ssao_gen, geo_depth);
  graph.PassReadsTexture(ssao_gen, ssao_noise);
  graph.PassWritesColor(ssao_gen, ssao_out);
  graph.SetPassViewport(ssao_gen,
                        {ctx.viewport_size.x / 2, ctx.viewport_size.y / 2});
  graph.SetPassClearColor(ssao_gen, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      ssao_gen,
      [this, renderer, ssao_out, geo_view_pos, geo_normal,
       geo_depth](RenderGraph& g) {
        auto color = g.GetTexture(ssao_out);
        auto view_pos = g.GetTexture(geo_view_pos);
        auto normal = g.GetTexture(geo_normal);
        auto depth = g.GetTexture(geo_depth);

        if (gen_bindings_.color_key != color.get()) {
          gen_bindings_.color_key = color.get();
        }

        if (gen_bindings_.view_pos_key != view_pos.get() ||
            gen_bindings_.normal_key != normal.get() ||
            gen_bindings_.depth_key != depth.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(renderer->GetDescriptorLayout("SSAOGen"));
          desc->AddCombinedImageSampler(0, view_pos->image_views_[0],
                                        renderer->GetDefaultNearestSampler());
          desc->AddCombinedImageSampler(1, normal->image_views_[0],
                                        renderer->GetDefaultNearestSampler());
          desc->AddCombinedImageSampler(2, depth->image_views_[0],
                                        renderer->GetDefaultNearestSampler());
          desc->AddCombinedImageSampler(
              3, renderer->GetSSAONoise()->image_views_[0],
              renderer->GetDefaultLinearSampler());
          desc->AddUniformBuffer(4, renderer->GetSSAOKernelUniformBuffer());
          desc->Bake();
          gen_bindings_.gen_desc = desc;
          gen_bindings_.view_pos_key = view_pos.get();
          gen_bindings_.normal_key = normal.get();
          gen_bindings_.depth_key = depth.get();
        }
      });

  // --- Blur H pass ---
  uint32_t ssao_blur_horz = graph.AddPass(
      "SSAO Blur H",
      [this, renderer](VkCommandBuffer) {
        blur_horz_pipeline_->Bind();
        renderer->DrawFullscreen(blur_horz_pipeline_,
                                 {blur_h_bindings_.input_desc});
      });
  graph.PassReadsTexture(ssao_blur_horz, ssao_out);
  graph.PassReadsTexture(ssao_blur_horz, geo_depth);
  graph.PassWritesColor(ssao_blur_horz, ssao_blur_h);
  graph.SetPassViewport(ssao_blur_horz, ctx.viewport_size);
  graph.SetPassClearColor(ssao_blur_horz, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      ssao_blur_horz,
      [this, renderer, ssao_blur_h, ssao_out, geo_depth](RenderGraph& g) {
        auto output = g.GetTexture(ssao_blur_h);
        auto input = g.GetTexture(ssao_out);
        auto depth = g.GetTexture(geo_depth);

        if (blur_h_bindings_.output_key != output.get()) {
          blur_h_bindings_.output_key = output.get();
        }

        if (blur_h_bindings_.input_key != input.get() ||
            blur_h_bindings_.depth_key != depth.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(renderer->GetDescriptorLayout("SSAOBlur"));
          desc->AddCombinedImageSampler(0, input->image_views_[0],
                                        renderer->GetDefaultLinearSampler());
          desc->AddCombinedImageSampler(1, depth->image_views_[0],
                                        renderer->GetDefaultNearestSampler());
          desc->Bake();
          blur_h_bindings_.input_desc = desc;
          blur_h_bindings_.input_key = input.get();
          blur_h_bindings_.depth_key = depth.get();
        }
      });

  // --- Blur V pass (writes the final output) ---
  uint32_t ssao_blur_vert = graph.AddPass(
      "SSAO Blur V",
      [this, renderer](VkCommandBuffer) {
        blur_vert_pipeline_->Bind();
        renderer->DrawFullscreen(blur_vert_pipeline_,
                                 {blur_v_bindings_.input_desc});
      });
  graph.PassReadsTexture(ssao_blur_vert, ssao_blur_h);
  graph.PassReadsTexture(ssao_blur_vert, geo_depth);
  graph.PassWritesColor(ssao_blur_vert, ssao_blur_v);
  graph.SetPassViewport(ssao_blur_vert, ctx.viewport_size);
  graph.SetPassClearColor(ssao_blur_vert, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      ssao_blur_vert,
      [this, renderer, pool, ssao_blur_v, ssao_blur_h,
       geo_depth](RenderGraph& g) {
        auto output = g.GetTexture(ssao_blur_v);
        auto input = g.GetTexture(ssao_blur_h);
        auto depth = g.GetTexture(geo_depth);

        if (blur_v_bindings_.output_key != output.get()) {
          blur_v_bindings_.output_key = output.get();
        }

        if (blur_v_bindings_.input_key != input.get() ||
            blur_v_bindings_.depth_key != depth.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(renderer->GetDescriptorLayout("SSAOBlur"));
          desc->AddCombinedImageSampler(0, input->image_views_[0],
                                        renderer->GetDefaultLinearSampler());
          desc->AddCombinedImageSampler(1, depth->image_views_[0],
                                        renderer->GetDefaultNearestSampler());
          desc->Bake();
          blur_v_bindings_.input_desc = desc;
          blur_v_bindings_.input_key = input.get();
          blur_v_bindings_.depth_key = depth.get();
        }

        // Publish the final SSAO output to the camera resource pool so
        // downstream features (Lighting, Composite) keep reading it by name.
        if (final_output_key_ != output.get() ||
            final_output_depth_key_ != depth.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(renderer->GetDescriptorLayout("SSAOBlur"));
          desc->AddCombinedImageSampler(0, output->image_views_[0],
                                        renderer->GetDefaultLinearSampler());
          desc->AddCombinedImageSampler(1, depth->image_views_[0],
                                        renderer->GetDefaultNearestSampler());
          desc->Bake();
          final_output_desc_ = desc;
          final_output_key_ = output.get();
          final_output_depth_key_ = depth.get();
        }
        pool->SetDescriptor("ssao.blur_v.output", final_output_desc_);
      });

  // Register final SSAO output for downstream features (e.g. Lighting)
  registry.Register("SSAOBlurV", ssao_blur_v);
}

}  // namespace wiesel
