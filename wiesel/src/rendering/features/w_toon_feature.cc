
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_toon_feature.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_scene.h"

namespace wiesel {

ToonFeature::ToonFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Postprocess render pass (1 color, no MSAA)
  render_pass_ =
      std::make_shared<RenderPass>(PassType::PostProcess, "Toon RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  // 3-sampler descriptor layout (scene color + normals + depth)
  toon_input_layout_ = std::make_shared<DescriptorSetLayout>();
  toon_input_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 VK_SHADER_STAGE_FRAGMENT_BIT);
  toon_input_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 VK_SHADER_STAGE_FRAGMENT_BIT);
  toon_input_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 VK_SHADER_STAGE_FRAGMENT_BIT);
  toon_input_layout_->Bake();

  // Pipeline
  push_constants_ = std::make_shared<ToonPushConstants>();
  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  auto frag = renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL,
                                       "main", ShaderSourceSource,
                                       "engine://shaders/toon_shader.frag"});
  pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, false, false, false});
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->AddInputLayout(toon_input_layout_);
  pipeline_->AddPushConstant(push_constants_, VK_SHADER_STAGE_FRAGMENT_BIT);
  pipeline_->AddShader(fullscreen_vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();
}

bool ToonFeature::IsEnabled(const RenderContext& ctx) const {
  return true;
}

void ToonFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("ToonFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  // Texture
  pool.SetTexture("toon.color", renderer.CreateAttachmentTexture(
                                    {rw, rh, AttachmentTextureType::Offscreen,
                                     1, renderer.GetSwapChainImageFormat(),
                                     SamplingMode::DISABLED, true}));

  // Framebuffer
  {
    std::array<AttachmentTexture*, 1> att{pool.GetTexture("toon.color").get()};
    pool.SetFramebuffer("toon",
                        render_pass_->CreateFramebuffer(0, att, {rw, rh}));
  }

  // Descriptors

  // Toon input: reads PipelineOutput + geometry normals + geometry depth
  auto toon_input_desc = std::make_shared<DescriptorSet>();
  toon_input_desc->SetLayout(toon_input_layout_);
  toon_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  toon_input_desc->AddCombinedImageSampler(
      1, pool.GetTexture("geometry.normal_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  toon_input_desc->AddCombinedImageSampler(
      2, pool.GetTexture("geometry.depth_resolve")->image_views_[0],
      renderer.GetDefaultNearestSampler());
  toon_input_desc->Bake();
  pool.SetDescriptor("toon.input", toon_input_desc);

  // Toon output descriptor: reads toon.color
  auto toon_output_desc = std::make_shared<DescriptorSet>();
  toon_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  toon_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("toon.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  toon_output_desc->Bake();
  pool.SetDescriptor("toon.output", toon_output_desc);

  // Update pipeline output for the next feature in the chain
  pool.SetTexture("PipelineOutput", pool.GetTexture("toon.color"));
  pool.SetDescriptor("PipelineOutputDescriptor", toon_output_desc);
}

void ToonFeature::AddPasses(RenderGraph& graph,
                            RenderResourceRegistry& registry,
                            RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("ToonFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto push_constants = push_constants_;
  glm::vec2 viewport_size = ctx.viewport_size;

  // Import toon output texture from pool
  RGResource toon_out =
      graph.ImportTexture("Toon", pool->GetTexture("toon.color"));

  // Get PipelineOutput from registry (set by previous feature)
  auto pipeline_input = registry.Get("PipelineOutput");

  // Also depend on the geometry pass outputs (normals + depth)
  auto geo_normal = registry.Get("GeoNormal");
  auto geo_depth = registry.Get("GeoDepth");

  // Toon pass
  auto pipeline = pipeline_;
  uint32_t toon_pass = graph.AddPass(
      "Toon", render_pass_,
      [pool, renderer, pipeline, push_constants,
       viewport_size](VkCommandBuffer) {
        pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(pipeline, {pool->GetDescriptor("toon.input")});
      });

  graph.PassReadsTexture(toon_pass, pipeline_input);
  graph.PassReadsTexture(toon_pass, geo_normal);
  graph.PassReadsTexture(toon_pass, geo_depth);
  graph.PassWritesColor(toon_pass, toon_out);
  graph.SetPassFramebuffer(toon_pass, pool->GetFramebuffer("toon"));
  graph.SetPassViewport(toon_pass, ctx.viewport_size);
  graph.SetPassClearColor(toon_pass, {0, 0, 0, 0});

  // Update PipelineOutput for the next feature in the chain
  registry.Register("PipelineOutput", toon_out);
}

}  // namespace wiesel
