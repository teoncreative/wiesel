
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
#include "rendering/w_renderpass.h"
#include "scene/w_scene.h"

namespace wiesel {

SSAOFeature::SSAOFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // SSAO Gen render pass (single R8_UNORM attachment)
  gen_render_pass_ = std::make_shared<RenderPass>(PassType::PostProcess,
                                                  "SSAO Generate RenderPass");
  gen_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                  .format = VK_FORMAT_R8_UNORM,
                                  .msaa_mode = SamplingMode::DISABLED});
  gen_render_pass_->Bake();

  blur_horz_render_pass_ = std::make_shared<RenderPass>(
      PassType::PostProcess, "SSAO Horizontal Blur RenderPass");
  blur_horz_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = VK_FORMAT_R8_UNORM,
       .msaa_mode = SamplingMode::DISABLED});
  blur_horz_render_pass_->Bake();

  blur_vert_render_pass_ = std::make_shared<RenderPass>(
      PassType::PostProcess, "SSAO Vertical Blur RenderPass");
  blur_vert_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = VK_FORMAT_R8_UNORM,
       .msaa_mode = SamplingMode::DISABLED});
  blur_vert_render_pass_->Bake();

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
  gen_pipeline_->SetRenderPass(gen_render_pass_);
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
  blur_horz_pipeline_->SetRenderPass(blur_horz_render_pass_);
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
  blur_vert_pipeline_->SetRenderPass(blur_vert_render_pass_);
  blur_vert_pipeline_->AddInputLayout(
      renderer_->GetDescriptorLayout("SSAOBlur"));
  blur_vert_pipeline_->AddShader(fullscreen_vert);
  blur_vert_pipeline_->AddShader(blur_v_frag);
  blur_vert_pipeline_->Bake();
}

bool SSAOFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.renderer.options().ssao_enabled;
}

void SSAOFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("SSAOFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  // SSAO gen output: half-resolution R8_UNORM
  pool.SetTexture("ssao.color",
                  renderer.CreateAttachmentTexture(
                      {rw / 2, rh / 2, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R8_UNORM, SamplingMode::DISABLED, true}));
  // SSAO blur H output: full-resolution R8_UNORM
  pool.SetTexture("ssao.blur_h",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R8_UNORM, SamplingMode::DISABLED, true}));
  // SSAO blur V output: full-resolution R8_UNORM
  pool.SetTexture("ssao.blur_v",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R8_UNORM, SamplingMode::DISABLED, true}));

  pool.SetFramebuffer("ssao.gen", gen_render_pass_->CreateFramebuffer(
                                      0, {pool.GetTexture("ssao.color").get()},
                                      {rw / 2, rh / 2}));
  pool.SetFramebuffer("ssao.blur_h",
                      blur_horz_render_pass_->CreateFramebuffer(
                          0, {pool.GetTexture("ssao.blur_h").get()}, {rw, rh}));
  pool.SetFramebuffer("ssao.blur_v",
                      blur_vert_render_pass_->CreateFramebuffer(
                          0, {pool.GetTexture("ssao.blur_v").get()}, {rw, rh}));

  // SSAO gen descriptor: reads geometry resolve textures + noise + kernel UBO
  auto ssao_gen_desc = std::make_shared<DescriptorSet>();
  ssao_gen_desc->SetLayout(renderer.GetDescriptorLayout("SSAOGen"));
  ssao_gen_desc->AddCombinedImageSampler(
      0, pool.GetTexture("geometry.view_pos_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  ssao_gen_desc->AddCombinedImageSampler(
      1, pool.GetTexture("geometry.normal_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  ssao_gen_desc->AddCombinedImageSampler(
      2, pool.GetTexture("geometry.depth_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  ssao_gen_desc->AddCombinedImageSampler(
      3, renderer.GetSSAONoise()->image_views_[0],
      renderer.GetDefaultLinearSampler());
  ssao_gen_desc->AddUniformBuffer(4, renderer.GetSSAOKernelUniformBuffer());
  ssao_gen_desc->Bake();
  pool.SetDescriptor("ssao.gen", ssao_gen_desc);

  // SSAO output descriptor: reads ssao.color + geometry depth (nearest sampler)
  auto ssao_output_desc = std::make_shared<DescriptorSet>();
  ssao_output_desc->SetLayout(renderer.GetDescriptorLayout("SSAOOutput"));
  ssao_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("ssao.color")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  ssao_output_desc->AddCombinedImageSampler(
      1, pool.GetTexture("geometry.depth_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  ssao_output_desc->Bake();
  pool.SetDescriptor("ssao.output", ssao_output_desc);

  // SSAO blur H output descriptor: reads ssao.blur_h (linear) + depth (nearest)
  auto blur_h_output_desc = std::make_shared<DescriptorSet>();
  blur_h_output_desc->SetLayout(renderer.GetDescriptorLayout("SSAOBlur"));
  blur_h_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("ssao.blur_h")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  blur_h_output_desc->AddCombinedImageSampler(
      1, pool.GetTexture("geometry.depth_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  blur_h_output_desc->Bake();
  pool.SetDescriptor("ssao.blur_h.output", blur_h_output_desc);

  // SSAO blur V output descriptor: reads ssao.blur_v (linear) + depth (nearest)
  auto blur_v_output_desc = std::make_shared<DescriptorSet>();
  blur_v_output_desc->SetLayout(renderer.GetDescriptorLayout("SSAOBlur"));
  blur_v_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("ssao.blur_v")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  blur_v_output_desc->AddCombinedImageSampler(
      1, pool.GetTexture("geometry.depth_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  blur_v_output_desc->Bake();
  pool.SetDescriptor("ssao.blur_v.output", blur_v_output_desc);
}

void SSAOFeature::AddPasses(RenderGraph& graph,
                            RenderResourceRegistry& registry,
                            RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("SSAOFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;

  // Import textures from pool
  RGResource ssao_noise =
      graph.ImportTexture("SSAONoise", renderer->GetSSAONoise());
  RGResource ssao_out =
      graph.ImportTexture("SSAOOut", pool->GetTexture("ssao.color"));
  RGResource ssao_blur_h =
      graph.ImportTexture("SSAOBlurH", pool->GetTexture("ssao.blur_h"));
  RGResource ssao_blur_v =
      graph.ImportTexture("SSAOBlurV", pool->GetTexture("ssao.blur_v"));

  // Get references to geometry outputs from the registry
  auto geo_view_pos = registry.Get("GeoViewPos");
  auto geo_normal = registry.Get("GeoNormal");
  auto geo_depth = registry.Get("GeoDepth");

  auto gen_pipeline = gen_pipeline_;
  uint32_t ssao_gen = graph.AddPass(
      "SSAO Gen", gen_render_pass_,
      [pool, renderer, gen_pipeline](VkCommandBuffer) {
        gen_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(gen_pipeline,
                                 {pool->GetDescriptor("ssao.gen"),
                                  pool->GetDescriptor("GlobalDescriptor")});
      });
  graph.PassReadsTexture(ssao_gen, geo_view_pos);
  graph.PassReadsTexture(ssao_gen, geo_normal);
  graph.PassReadsTexture(ssao_gen, geo_depth);
  graph.PassReadsTexture(ssao_gen, ssao_noise);
  graph.PassWritesColor(ssao_gen, ssao_out);
  graph.SetPassFramebuffer(ssao_gen, pool->GetFramebuffer("ssao.gen"));
  graph.SetPassViewport(ssao_gen,
                        {ctx.viewport_size.x / 2, ctx.viewport_size.y / 2});
  graph.SetPassClearColor(ssao_gen, {0, 0, 0, 0});

  auto blur_h_pipeline = blur_horz_pipeline_;
  uint32_t ssao_blur_horz = graph.AddPass(
      "SSAO Blur H", blur_horz_render_pass_,
      [pool, renderer, blur_h_pipeline](VkCommandBuffer) {
        blur_h_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(blur_h_pipeline,
                                 {pool->GetDescriptor("ssao.output")});
      });
  graph.PassReadsTexture(ssao_blur_horz, ssao_out);
  graph.PassReadsTexture(ssao_blur_horz, geo_depth);
  graph.PassWritesColor(ssao_blur_horz, ssao_blur_h);
  graph.SetPassFramebuffer(ssao_blur_horz, pool->GetFramebuffer("ssao.blur_h"));
  graph.SetPassViewport(ssao_blur_horz, ctx.viewport_size);
  graph.SetPassClearColor(ssao_blur_horz, {0, 0, 0, 0});

  auto blur_v_pipeline = blur_vert_pipeline_;
  uint32_t ssao_blur_vert = graph.AddPass(
      "SSAO Blur V", blur_vert_render_pass_,
      [pool, renderer, blur_v_pipeline](VkCommandBuffer) {
        blur_v_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(blur_v_pipeline,
                                 {pool->GetDescriptor("ssao.blur_h.output")});
      });
  graph.PassReadsTexture(ssao_blur_vert, ssao_blur_h);
  graph.PassReadsTexture(ssao_blur_vert, geo_depth);
  graph.PassWritesColor(ssao_blur_vert, ssao_blur_v);
  graph.SetPassFramebuffer(ssao_blur_vert, pool->GetFramebuffer("ssao.blur_v"));
  graph.SetPassViewport(ssao_blur_vert, ctx.viewport_size);
  graph.SetPassClearColor(ssao_blur_vert, {0, 0, 0, 0});

  // Register final SSAO output for downstream features (e.g. Lighting)
  registry.Register("SSAOBlurV", ssao_blur_v);
}

}  // namespace wiesel
