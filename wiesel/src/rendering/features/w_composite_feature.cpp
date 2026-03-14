
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_composite_feature.hpp"
#include "rendering/w_renderer.hpp"
#include "rendering/w_pipeline.hpp"
#include "rendering/w_renderpass.hpp"
#include "scene/w_scene.hpp"

namespace Wiesel {

CompositeFeature::CompositeFeature(Ref<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Render pass (1 color + optional resolve for MSAA)
  render_pass_ = CreateReference<RenderPass>(PassType::PostProcess,
                                             "Composite RenderPass");
  render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = renderer_->GetSwapChainImageFormat(),
       .msaa_mode = renderer_->options().msaa_mode});
  if (renderer_->options().msaa_mode > SamplingMode::DISABLED) {
    render_pass_->AttachOutput(
        {.type = AttachmentTextureType::Resolve,
         .format = renderer_->GetSwapChainImageFormat(),
         .msaa_mode = SamplingMode::DISABLED});
  }
  render_pass_->Bake();

  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/internal_shaders/fullscreen_shader.vert"});
  auto composite_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/internal_shaders/quad_shader.frag"});
  pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      renderer_->options().msaa_mode, CullModeFront, false, true, true, false});
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->AddInputLayout(renderer_->GetSkyboxDescriptorLayout());
  pipeline_->AddShader(fullscreen_vert);
  pipeline_->AddShader(composite_frag);
  pipeline_->Bake();
}

void CompositeFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("CompositeFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  SamplingMode msaa = renderer.options().msaa_mode;
  bool use_msaa = msaa > SamplingMode::DISABLED;

  // Textures
  pool.SetTexture("composite.color", renderer.CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       renderer.GetSwapChainImageFormat(), msaa,
       msaa == SamplingMode::DISABLED}));

  if (use_msaa) {
    pool.SetTexture("composite.color_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED,
             true}));

    std::array<AttachmentTexture*, 2> textures{
        pool.GetTexture("composite.color").get(),
        pool.GetTexture("composite.color_resolve").get()};
    pool.SetFramebuffer("composite",
        render_pass_->CreateFramebuffer(0, textures, ctx.viewport_size));
  } else {
    pool.SetTexture("composite.color_resolve",
                    pool.GetTexture("composite.color"));

    std::array<AttachmentTexture*, 1> textures{
        pool.GetTexture("composite.color").get()};
    pool.SetFramebuffer("composite",
        render_pass_->CreateFramebuffer(0, textures, ctx.viewport_size));
  }

  // Composite output descriptor: reads resolved composite color, linear sampler
  auto composite_output_desc = CreateReference<DescriptorSet>();
  composite_output_desc->SetLayout(renderer.GetPresentDescriptorLayout());
  composite_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("composite.color_resolve")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  composite_output_desc->Bake();
  pool.SetDescriptor("composite.output", composite_output_desc);

  // Set initial PipelineOutput for the post-processing chain
  pool.SetTexture("PipelineOutput",
                  pool.GetTexture("composite.color_resolve"));
  pool.SetDescriptor("PipelineOutputDescriptor",
                     composite_output_desc);
}

void CompositeFeature::AddPasses(RenderGraph& graph,
                                 RenderResourceRegistry& registry,
                                 RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("CompositeFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  bool use_resolve = ctx.use_msaa_resolve;

  // Import composite output texture from pool
  auto composite_tex = use_resolve
      ? pool->GetTexture("composite.color_resolve")
      : pool->GetTexture("composite.color");
  RGResource composite_out =
      graph.ImportTexture("CompositeOut", composite_tex);

  // Get references to lighting, transparency, and sprite outputs from the registry
  auto lighting_out = registry.Get("LightingOut");
  auto transparency_out = registry.Has("TransparencyOut") ? registry.Get("TransparencyOut") : RGResource{};
  auto sprite_out = registry.Get("SpriteOut");

  // Composite pass (canvas is blended later by CanvasFeature)
  auto pipeline = pipeline_;
  uint32_t composite = graph.AddPass(
      "Composite", render_pass_,
      [pipeline, pool, renderer](VkCommandBuffer) {
        pipeline->Bind(PipelineBindPointGraphics);
        if (renderer->options().only_ssao) {
          renderer->DrawFullscreen(pipeline,
              {pool->GetDescriptor("ssao.blur_v.output")});
        } else {
          renderer->DrawFullscreen(pipeline,
              {pool->GetDescriptor("lighting.output")});
          if (pool->HasDescriptor("transparency.output")) {
            renderer->DrawFullscreen(pipeline,
                {pool->GetDescriptor("transparency.output")});
          }
          renderer->DrawFullscreen(pipeline,
              {pool->GetDescriptor("sprite.output")});
        }
      });

  graph.PassReadsTexture(composite, lighting_out);
  if (transparency_out.IsValid()) {
    graph.PassReadsTexture(composite, transparency_out);
  }
  if (sprite_out.IsValid()) {
    graph.PassReadsTexture(composite, sprite_out);
  }
  graph.PassWritesColor(composite, composite_out);
  graph.SetPassFramebuffer(composite, pool->GetFramebuffer("composite"));
  graph.SetPassViewport(composite, ctx.viewport_size);
  graph.SetPassClearColor(composite, renderer->GetClearColor());

  // Register outputs for downstream features
  registry.Register("CompositeOut", composite_out);
  registry.Register("PipelineOutput", composite_out);
}

}  // namespace Wiesel
