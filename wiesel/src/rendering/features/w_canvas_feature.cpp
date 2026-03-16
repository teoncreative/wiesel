
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_canvas_feature.hpp"
#include <algorithm>
#include <unordered_set>
#include "rendering/w_pipeline.hpp"
#include "rendering/w_renderer.hpp"
#include "rendering/w_renderpass.hpp"
#include "scene/w_scene.hpp"
#include "ui/w_canvas.hpp"
#include "ui/w_font.hpp"

namespace Wiesel {

CanvasFeature::CanvasFeature(Ref<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Render pass: single offscreen RGBA attachment, no MSAA
  render_pass_ =
      CreateReference<RenderPass>(PassType::PostProcess, "Canvas RenderPass");
  render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = renderer_->GetSwapChainImageFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  // Descriptor layouts
  canvas_element_layout_ = CreateReference<DescriptorSetLayout>();
  canvas_element_layout_->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     VK_SHADER_STAGE_VERTEX_BIT);
  canvas_element_layout_->Bake();

  canvas_textured_layout_ = CreateReference<DescriptorSetLayout>();
  canvas_textured_layout_->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_VERTEX_BIT);
  canvas_textured_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      VK_SHADER_STAGE_FRAGMENT_BIT);
  canvas_textured_layout_->Bake();

  // Shaders
  auto canvas_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/canvas_shader.vert"});
  auto rect_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/canvas_rect.frag"});
  auto image_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/canvas_image.frag"});
  auto text_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/canvas_text.frag"});

  // Push constant for screen size
  screen_size_push_ = CreateReference<CanvasScreenPushConstant>();

  // Pipeline properties: alpha blending, no depth, no culling, no vertex input
  PipelineProperties props{SamplingMode::DISABLED, CullModeNone, false, true,
                           false, false};

  // Rect pipeline (UBO only)
  rect_pipeline_ = CreateReference<Pipeline>(props);
  rect_pipeline_->SetRenderPass(render_pass_);
  rect_pipeline_->AddInputLayout(canvas_element_layout_);
  rect_pipeline_->AddShader(canvas_vert);
  rect_pipeline_->AddShader(rect_frag);
  rect_pipeline_->AddPushConstant(screen_size_push_,
                                  VK_SHADER_STAGE_VERTEX_BIT);
  rect_pipeline_->Bake();

  // Image pipeline (UBO + texture sampler)
  image_pipeline_ = CreateReference<Pipeline>(props);
  image_pipeline_->SetRenderPass(render_pass_);
  image_pipeline_->AddInputLayout(canvas_textured_layout_);
  image_pipeline_->AddShader(canvas_vert);
  image_pipeline_->AddShader(image_frag);
  image_pipeline_->AddPushConstant(screen_size_push_,
                                   VK_SHADER_STAGE_VERTEX_BIT);
  image_pipeline_->Bake();

  // Text pipeline (UBO + font atlas sampler)
  text_pipeline_ = CreateReference<Pipeline>(props);
  text_pipeline_->SetRenderPass(render_pass_);
  text_pipeline_->AddInputLayout(canvas_textured_layout_);
  text_pipeline_->AddShader(canvas_vert);
  text_pipeline_->AddShader(text_frag);
  text_pipeline_->AddPushConstant(screen_size_push_,
                                  VK_SHADER_STAGE_VERTEX_BIT);
  text_pipeline_->Bake();

  // Composite pass: blends canvas offscreen onto PipelineOutput
  comp_render_pass_ = CreateReference<RenderPass>(
      PassType::PostProcess, "CanvasComposite RenderPass");
  comp_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = renderer_->GetSwapChainImageFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  comp_render_pass_->Bake();

  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/fullscreen_shader.vert"});
  auto quad_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/quad_shader.frag"});
  comp_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, true, true, false});
  comp_pipeline_->SetRenderPass(comp_render_pass_);
  comp_pipeline_->AddInputLayout(renderer_->GetPresentDescriptorLayout());
  comp_pipeline_->AddShader(fullscreen_vert);
  comp_pipeline_->AddShader(quad_frag);
  comp_pipeline_->Bake();
}

void CanvasFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("CanvasFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  pool.SetTexture("canvas.color",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       renderer.GetSwapChainImageFormat(),
                       SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 1> attachments{
      pool.GetTexture("canvas.color").get()};
  pool.SetFramebuffer(
      "canvas", render_pass_->CreateFramebuffer(0, attachments, {rw, rh}));

  auto canvas_output_desc = CreateReference<DescriptorSet>();
  canvas_output_desc->SetLayout(renderer.GetPresentDescriptorLayout());
  canvas_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("canvas.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  canvas_output_desc->Bake();
  pool.SetDescriptor("canvas.output", canvas_output_desc);

  // Canvas composite: blend canvas onto PipelineOutput
  pool.SetTexture("canvas_comp.color",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       renderer.GetSwapChainImageFormat(),
                       SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 1> comp_attachments{
      pool.GetTexture("canvas_comp.color").get()};
  pool.SetFramebuffer("canvas_comp",
      comp_render_pass_->CreateFramebuffer(0, comp_attachments, {rw, rh}));

  // Descriptor to read previous PipelineOutput
  auto comp_input_desc = CreateReference<DescriptorSet>();
  comp_input_desc->SetLayout(renderer.GetPresentDescriptorLayout());
  comp_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  comp_input_desc->Bake();
  pool.SetDescriptor("canvas_comp.input", comp_input_desc);

  // Update PipelineOutput for downstream features
  auto comp_output_desc = CreateReference<DescriptorSet>();
  comp_output_desc->SetLayout(renderer.GetPresentDescriptorLayout());
  comp_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("canvas_comp.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  comp_output_desc->Bake();
  pool.SetTexture("PipelineOutput",
                  pool.GetTexture("canvas_comp.color"));
  pool.SetDescriptor("PipelineOutputDescriptor", comp_output_desc);
}

void CanvasFeature::AddPasses(RenderGraph& graph,
                              RenderResourceRegistry& registry,
                              RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("CanvasFeature::AddPasses");
  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto* scene = &ctx.scene;
  auto rect_pipeline = rect_pipeline_;
  auto image_pipeline = image_pipeline_;
  auto text_pipeline = text_pipeline_;
  auto screen_push = screen_size_push_;
  auto viewport = ctx.viewport_size;
  auto element_layout = canvas_element_layout_;
  auto textured_layout = canvas_textured_layout_;

  // Pre-process text: load fonts, rasterize any new glyphs, and upload
  // atlases BEFORE command recording begins.  GPU resource creation
  // (texture upload) is invalid inside a render pass.
  //
  // Phase 1: Touch all codepoints across all text entities so any
  // on-demand rasterization happens now.  Collect unique fonts.
  std::unordered_map<Font*, Ref<Font>> unique_fonts;
  for (const auto& entity :
       ctx.scene.GetAllEntitiesWith<TextComponent,
                                     RectangleTransformComponent>()) {
    auto& text = ctx.scene.GetComponent<TextComponent>(entity);
    if (text.text.empty()) {
      continue;
    }
    Ref<Font> font = FontCache::Get(text.font_path, text.font_size);
    if (!font || !font->IsLoaded()) {
      continue;
    }
    for (size_t i = 0; i < text.text.size();) {
      uint32_t cp = Font::DecodeUTF8(text.text, i);
      font->GetGlyph(cp);
    }
    unique_fonts[font.get()] = font;
  }

  // Phase 2: Flush all dirty font atlases.
  std::unordered_set<Font*> flushed_fonts;
  for (auto& [ptr, font] : unique_fonts) {
    if (font->FlushAtlas()) {
      flushed_fonts.insert(ptr);
    }
  }

  // Phase 3: Invalidate GPU descriptors for ALL text entities that use
  // a font whose atlas was re-uploaded (new VkImage = old descriptors invalid).
  if (!flushed_fonts.empty()) {
    for (const auto& entity :
         ctx.scene.GetAllEntitiesWith<TextComponent,
                                       RectangleTransformComponent>()) {
      auto& text = ctx.scene.GetComponent<TextComponent>(entity);
      Ref<Font> font = FontCache::Get(text.font_path, text.font_size);
      if (font && flushed_fonts.contains(font.get())) {
        text.glyph_gpu_.clear();
      }
    }
  }

  RGResource canvas_out =
      graph.ImportTexture("CanvasOut", pool->GetTexture("canvas.color"));

  // Collect all canvas-drawable entities and sort by draw_order so children
  // render on top of parents regardless of component type.
  enum class CanvasElementType { Rect, Image, Text };
  struct CanvasDrawEntry {
    entt::entity entity;
    CanvasElementType type;
    int32_t draw_order;
  };
  std::vector<CanvasDrawEntry> draw_list;

  for (const auto& entity :
       ctx.scene.GetAllEntitiesWith<CanvasRectComponent,
                                     RectangleTransformComponent>()) {
    auto& rt = ctx.scene.GetComponent<RectangleTransformComponent>(entity);
    draw_list.push_back({entity, CanvasElementType::Rect, rt.draw_order});
  }
  for (const auto& entity :
       ctx.scene.GetAllEntitiesWith<CanvasImageComponent,
                                     RectangleTransformComponent>()) {
    auto& rt = ctx.scene.GetComponent<RectangleTransformComponent>(entity);
    draw_list.push_back({entity, CanvasElementType::Image, rt.draw_order});
  }
  for (const auto& entity :
       ctx.scene.GetAllEntitiesWith<TextComponent,
                                     RectangleTransformComponent>()) {
    auto& rt = ctx.scene.GetComponent<RectangleTransformComponent>(entity);
    draw_list.push_back({entity, CanvasElementType::Text, rt.draw_order});
  }
  std::sort(draw_list.begin(), draw_list.end(),
            [](const CanvasDrawEntry& a, const CanvasDrawEntry& b) {
              return a.draw_order < b.draw_order;
            });

  auto sorted_list = std::make_shared<std::vector<CanvasDrawEntry>>(
      std::move(draw_list));

  uint32_t canvas_pass = graph.AddPass(
      "Canvas", render_pass_,
      [rect_pipeline, image_pipeline, text_pipeline, scene, renderer, pool,
       screen_push, viewport, element_layout, textured_layout,
       sorted_list](VkCommandBuffer) {
        screen_push->screen_size = viewport;

        CanvasElementType bound_type = CanvasElementType::Rect;
        bool first = true;

        for (const auto& entry : *sorted_list) {
          // Rebind pipeline when element type changes
          if (first || entry.type != bound_type) {
            switch (entry.type) {
              case CanvasElementType::Rect:
                rect_pipeline->Bind(PipelineBindPointGraphics);
                break;
              case CanvasElementType::Image:
                image_pipeline->Bind(PipelineBindPointGraphics);
                break;
              case CanvasElementType::Text:
                text_pipeline->Bind(PipelineBindPointGraphics);
                break;
            }
            bound_type = entry.type;
            first = false;
          }

          switch (entry.type) {
            case CanvasElementType::Rect: {
              auto& rect =
                  scene->GetComponent<CanvasRectComponent>(entry.entity);
              auto& rt = scene->GetComponent<RectangleTransformComponent>(
                  entry.entity);
              renderer->DrawCanvasRect(rt, rect, element_layout);
              break;
            }
            case CanvasElementType::Image: {
              auto& img =
                  scene->GetComponent<CanvasImageComponent>(entry.entity);
              auto& rt = scene->GetComponent<RectangleTransformComponent>(
                  entry.entity);
              renderer->DrawCanvasImage(rt, img, textured_layout);
              break;
            }
            case CanvasElementType::Text: {
              auto& text = scene->GetComponent<TextComponent>(entry.entity);
              auto& rt = scene->GetComponent<RectangleTransformComponent>(
                  entry.entity);
              renderer->DrawCanvasText(rt, text, textured_layout);
              break;
            }
          }
        }
      });

  graph.PassWritesColor(canvas_pass, canvas_out);
  graph.SetPassFramebuffer(canvas_pass, pool->GetFramebuffer("canvas"));
  graph.SetPassViewport(canvas_pass, ctx.viewport_size);
  graph.SetPassClearColor(canvas_pass, {0, 0, 0, 0});

  registry.Register("CanvasOut", canvas_out);

  // Second pass: composite canvas onto PipelineOutput (after all post-processing)
  RGResource canvas_comp_out =
      graph.ImportTexture("CanvasComp", pool->GetTexture("canvas_comp.color"));
  auto pipeline_input = registry.Get("PipelineOutput");
  auto comp_pipeline = comp_pipeline_;

  uint32_t canvas_comp = graph.AddPass(
      "CanvasComposite", comp_render_pass_,
      [comp_pipeline, pool, renderer](VkCommandBuffer) {
        comp_pipeline->Bind(PipelineBindPointGraphics);
        // Draw scene (previous PipelineOutput)
        renderer->DrawFullscreen(comp_pipeline,
            {pool->GetDescriptor("canvas_comp.input")});
        // Draw canvas on top (alpha blended)
        renderer->DrawFullscreen(comp_pipeline,
            {pool->GetDescriptor("canvas.output")});
      });

  graph.PassReadsTexture(canvas_comp, pipeline_input);
  graph.PassReadsTexture(canvas_comp, canvas_out);
  graph.PassWritesColor(canvas_comp, canvas_comp_out);
  graph.SetPassFramebuffer(canvas_comp, pool->GetFramebuffer("canvas_comp"));
  graph.SetPassViewport(canvas_comp, ctx.viewport_size);
  graph.SetPassClearColor(canvas_comp, {0, 0, 0, 0});

  registry.Register("PipelineOutput", canvas_comp_out);
}

}  // namespace Wiesel
