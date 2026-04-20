
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
#include "scene/w_scene.h"

namespace wiesel {

ToonFeature::ToonFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
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
  pipeline_->AddColorAttachment(renderer_->GetSwapChainImageFormat());
  pipeline_->AddInputLayout(toon_input_layout_);
  pipeline_->AddPushConstant(push_constants_, VK_SHADER_STAGE_FRAGMENT_BIT);
  pipeline_->AddShader(fullscreen_vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();
}

bool ToonFeature::IsEnabled(const RenderContext& ctx) const {
  return true;
}

void ToonFeature::SetupResources(RenderContext& /*ctx*/) {}

void ToonFeature::AddPasses(RenderGraph& graph,
                            RenderResourceRegistry& registry,
                            RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("ToonFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto push_constants = push_constants_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  RGResource toon_out = graph.DeclareTransient(RGTextureDesc{
      .name = "toon.color",
      .width = rw,
      .height = rh,
      .format = renderer->GetSwapChainImageFormat(),
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true});

  RGResource pipeline_input = registry.Get("PipelineOutput");
  RGResource geo_normal = registry.Get("GeoNormal");
  RGResource geo_depth = registry.Get("GeoDepth");

  auto pipeline = pipeline_;
  auto input_layout = toon_input_layout_;
  uint32_t toon_pass = graph.AddPass(
      "Toon", [this, renderer, pipeline, push_constants](VkCommandBuffer) {
        pipeline->Bind();
        renderer->DrawFullscreen(pipeline, {input_desc_});
      });
  graph.PassReadsTexture(toon_pass, pipeline_input);
  graph.PassReadsTexture(toon_pass, geo_normal);
  graph.PassReadsTexture(toon_pass, geo_depth);
  graph.PassWritesColor(toon_pass, toon_out);
  graph.SetPassViewport(toon_pass, ctx.viewport_size);
  graph.SetPassClearColor(toon_pass, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      toon_pass,
      [this, renderer, pool, toon_out, pipeline_input, geo_normal, geo_depth,
       input_layout](RenderGraph& g) {
        auto output = g.GetTexture(toon_out);
        auto input = g.GetTexture(pipeline_input);
        auto normal = g.GetTexture(geo_normal);
        auto depth = g.GetTexture(geo_depth);
        auto linear = renderer->GetDefaultLinearSampler();
        auto nearest = renderer->GetDefaultNearestSampler();
        auto present_layout = renderer->GetDescriptorLayout("Present");

        if (output_key_ != output.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(present_layout);
          desc->AddCombinedImageSampler(0, output->image_views_[0], linear);
          desc->Bake();
          output_desc_ = desc;
          output_key_ = output.get();
        }

        if (input_key_ != input.get() || normal_key_ != normal.get() ||
            depth_key_ != depth.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(input_layout);
          desc->AddCombinedImageSampler(0, input->image_views_[0], linear);
          desc->AddCombinedImageSampler(1, normal->image_views_[0], nearest);
          desc->AddCombinedImageSampler(2, depth->image_views_[0], nearest);
          desc->Bake();
          input_desc_ = desc;
          input_key_ = input.get();
          normal_key_ = normal.get();
          depth_key_ = depth.get();
        }

        pool->SetTexture("PipelineOutput", output);
        pool->SetDescriptor("PipelineOutputDescriptor", output_desc_);
      });

  registry.Register("PipelineOutput", toon_out);
}

}  // namespace wiesel
