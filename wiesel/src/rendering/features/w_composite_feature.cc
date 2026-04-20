
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_composite_feature.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "scene/w_scene.h"

namespace wiesel {

CompositeFeature::CompositeFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  auto composite_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/quad_shader.frag"});
  pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      renderer_->options().msaa_mode, CullModeBack, false, true, true, false});
  pipeline_->AddColorAttachment(renderer_->GetSwapChainImageFormat());
  pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Skybox"));
  pipeline_->AddShader(fullscreen_vert);
  pipeline_->AddShader(composite_frag);
  pipeline_->Bake();
}

void CompositeFeature::SetupResources(RenderContext& /*ctx*/) {}

void CompositeFeature::AddPasses(RenderGraph& graph,
                                 RenderResourceRegistry& registry,
                                 RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("CompositeFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  SamplingMode msaa = renderer->options().msaa_mode;
  bool use_msaa = msaa > SamplingMode::DISABLED;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  // MSAA color cannot be sampled directly; downstream reads color_resolve.
  RGResource composite_color = graph.DeclareTransient(RGTextureDesc{
      .name = "composite.color",
      .width = rw,
      .height = rh,
      .format = renderer->GetSwapChainImageFormat(),
      .samples = msaa,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = !use_msaa});

  RGResource composite_resolve = use_msaa
                                     ? graph.DeclareTransient(RGTextureDesc{
                                           .name = "composite.color_resolve",
                                           .width = rw,
                                           .height = rh,
                                           .format =
                                               renderer->GetSwapChainImageFormat(),
                                           .samples = SamplingMode::DISABLED,
                                           .type = AttachmentTextureType::Resolve,
                                           .layer_count = 1,
                                           .sampled = true})
                                     : composite_color;

  RGResource lighting_out = registry.Get("LightingOut");
  RGResource transparency_out = registry.Has("TransparencyOut")
                                    ? registry.Get("TransparencyOut")
                                    : RGResource{};
  RGResource sprite_out = registry.Get("SpriteOut");
  RGResource grid_out =
      registry.Has("GridOut") ? registry.Get("GridOut") : RGResource{};

  auto pipeline = pipeline_;
  bool has_transparency = transparency_out.IsValid();
  bool has_grid = grid_out.IsValid();
  uint32_t composite = graph.AddPass(
      "Composite",
      [pipeline, pool, renderer, has_transparency, has_grid](VkCommandBuffer) {
        pipeline->Bind();
        if (renderer->options().only_ssao) {
          renderer->DrawFullscreen(pipeline,
                                   {pool->GetDescriptor("ssao.blur_v.output")});
        } else {
          renderer->DrawFullscreen(pipeline,
                                   {pool->GetDescriptor("lighting.output")});
          if (has_grid) {
            renderer->DrawFullscreen(pipeline,
                                     {pool->GetDescriptor("grid.output")});
          }
          if (has_transparency) {
            renderer->DrawFullscreen(
                pipeline, {pool->GetDescriptor("transparency.output")});
          }
          renderer->DrawFullscreen(pipeline,
                                   {pool->GetDescriptor("sprite.output")});
        }
      });
  graph.PassReadsTexture(composite, lighting_out);
  if (grid_out.IsValid()) {
    graph.PassReadsTexture(composite, grid_out);
  }
  if (transparency_out.IsValid()) {
    graph.PassReadsTexture(composite, transparency_out);
  }
  if (sprite_out.IsValid()) {
    graph.PassReadsTexture(composite, sprite_out);
  }
  if (use_msaa) {
    graph.PassWritesColor(composite, composite_color, composite_resolve);
  } else {
    graph.PassWritesColor(composite, composite_color);
  }
  graph.SetPassViewport(composite, ctx.viewport_size);
  graph.SetPassClearColor(composite, renderer->GetClearColor());
  graph.SetPassResolveFn(
      composite,
      [this, renderer, pool, composite_resolve](RenderGraph& g) {
        auto resolve = g.GetTexture(composite_resolve);
        auto linear = renderer->GetDefaultLinearSampler();
        auto present_layout = renderer->GetDescriptorLayout("Present");

        if (resolve_key_ != resolve.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(present_layout);
          desc->AddCombinedImageSampler(0, resolve->image_views_[0], linear);
          desc->Bake();
          output_desc_ = desc;
          resolve_key_ = resolve.get();
        }

        pool->SetTexture("PipelineOutput", resolve);
        pool->SetDescriptor("PipelineOutputDescriptor", output_desc_);
      });

  // Register outputs for downstream features
  registry.Register("CompositeOut", composite_resolve);
  registry.Register("PipelineOutput", composite_resolve);
}

}  // namespace wiesel
