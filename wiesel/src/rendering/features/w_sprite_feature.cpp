
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_sprite_feature.hpp"
#include "rendering/w_pipeline.hpp"
#include "rendering/w_renderer.hpp"
#include "rendering/w_renderpass.hpp"
#include "rendering/w_sprite.hpp"
#include "scene/w_scene.hpp"

namespace Wiesel {

SpriteFeature::SpriteFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Render pass (single swap-chain-format attachment, no MSAA)
  render_pass_ =
      std::make_shared<RenderPass>(PassType::PostProcess, "Sprite RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  auto sprite_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/sprite_shader.vert"});
  auto sprite_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/sprite_shader.frag"});
  pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeNone, false, true, false, false});
  pipeline_->SetVertexData(VertexSprite::GetBindingDescriptions(),
                           VertexSprite::GetAttributeDescriptions());
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->AddInputLayout(renderer_->GetSpriteDrawDescriptorLayout());
  pipeline_->AddInputLayout(renderer_->GetGlobalDescriptorLayout());
  pipeline_->AddShader(sprite_vert);
  pipeline_->AddShader(sprite_frag);
  pipeline_->Bake();
}

void SpriteFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("SpriteFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  pool.SetTexture("sprite.color", renderer.CreateAttachmentTexture(
                                      {rw, rh, AttachmentTextureType::Offscreen,
                                       1, renderer.GetSwapChainImageFormat(),
                                       SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 1> attachments{
      pool.GetTexture("sprite.color").get()};
  pool.SetFramebuffer(
      "sprite", render_pass_->CreateFramebuffer(0, attachments, {rw, rh}));

  // Sprite output descriptor: reads sprite.color, linear sampler
  auto sprite_output_desc = std::make_shared<DescriptorSet>();
  sprite_output_desc->SetLayout(renderer.GetPresentDescriptorLayout());
  sprite_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("sprite.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  sprite_output_desc->Bake();
  pool.SetDescriptor("sprite.output", sprite_output_desc);
}

void SpriteFeature::AddPasses(RenderGraph& graph,
                              RenderResourceRegistry& registry,
                              RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("SpriteFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto* scene = &ctx.scene;
  auto pipeline = pipeline_;

  // Import sprite output texture from pool
  RGResource sprite_out =
      graph.ImportTexture("SpriteOut", pool->GetTexture("sprite.color"));

  uint32_t sprite = graph.AddPass(
      "Sprite", render_pass_, [pipeline, scene, renderer](VkCommandBuffer) {
        pipeline->Bind(PipelineBindPointGraphics);

        // Collect and sort sprites by sort_layer
        struct SpriteEntry {
          entt::entity entity;
          uint8_t layer;
        };
        std::vector<SpriteEntry> sorted;
        for (const auto& entity :
             scene->GetAllEntitiesWith<SpriteComponent, TransformComponent>()) {
          auto& spr = scene->GetComponent<SpriteComponent>(entity);
          sorted.push_back({entity, spr.sort_layer_});
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const SpriteEntry& a, const SpriteEntry& b) {
                    return a.layer < b.layer;
                  });

        for (auto& entry : sorted) {
          auto& spr = scene->GetComponent<SpriteComponent>(entry.entity);
          auto& transform =
              scene->GetComponent<TransformComponent>(entry.entity);
          renderer->DrawSprite(spr, transform);
        }
      });

  graph.PassWritesColor(sprite, sprite_out);
  graph.SetPassFramebuffer(sprite, pool->GetFramebuffer("sprite"));
  graph.SetPassViewport(sprite, ctx.viewport_size);
  graph.SetPassClearColor(sprite, {0, 0, 0, 0});

  // Register output for downstream features (e.g. Composite)
  registry.Register("SpriteOut", sprite_out);
}

}  // namespace Wiesel
