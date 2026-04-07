
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_canvas_feature.h"
#include <algorithm>
#include <unordered_set>
#include "asset/w_asset_manager.h"
#include "rendering/w_camera.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_components.h"
#include "scene/w_entity.h"
#include "scene/w_scene.h"
#include "ui/w_canvas.h"
#include "ui/w_font.h"
#include "ui/w_ui_document.h"
#include "ui/w_ui_manager.h"
#include "w_engine.h"

namespace Wiesel {

// Collect all canvas-drawable entities and sort by draw_order so children
// render on top of parents regardless of component type.
enum class CanvasElementType { Rect, Image, Button, Text, UIDocument };

struct CanvasDrawEntry {
  Scene* scene;
  entt::entity entity;
  entt::entity canvas_root;
  CanvasElementType type;
  int32_t draw_order;
};

// Resolve the active texture and tint for a button's current state.
static std::pair<AssetHandle, glm::vec4> GetButtonVisuals(
    const ButtonComponent& btn) {
  switch (btn.state_) {
    case ButtonState::Hovered:
      return {btn.hovered_texture.IsValid() ? btn.hovered_texture
                                            : btn.normal_texture,
              btn.hovered_color};
    case ButtonState::Pressed:
      return {btn.pressed_texture.IsValid() ? btn.pressed_texture
                                            : btn.normal_texture,
              btn.pressed_color};
    case ButtonState::Selected:
      return {btn.selected_texture.IsValid() ? btn.selected_texture
                                             : btn.normal_texture,
              btn.selected_color};
    case ButtonState::Disabled:
      return {btn.disabled_texture.IsValid() ? btn.disabled_texture
                                             : btn.normal_texture,
              btn.disabled_color};
    default:
      return {btn.normal_texture, btn.normal_color};
  }
}

// Draw a single canvas element (Rect, Image, Button, or Text).
// Uses the Scene* stored in the entry so each entity is looked up in the
// correct scene when multiple scenes are active.
static void DrawCanvasElement(
    const CanvasDrawEntry& entry, std::shared_ptr<Renderer> renderer,
    std::shared_ptr<DescriptorSetLayout> element_layout,
    std::shared_ptr<DescriptorSetLayout> textured_layout, uint32_t eid) {
  Scene* scene = entry.scene;
  switch (entry.type) {
    case CanvasElementType::Rect: {
      auto& rect = scene->GetComponent<CanvasRectComponent>(entry.entity);
      auto& rt = scene->GetComponent<RectangleTransformComponent>(entry.entity);
      renderer->DrawCanvasRect(rt, rect, element_layout, eid);
      break;
    }
    case CanvasElementType::Image: {
      auto& img = scene->GetComponent<CanvasImageComponent>(entry.entity);
      auto& rt = scene->GetComponent<RectangleTransformComponent>(entry.entity);
      auto texture =
          Engine::asset_manager().GetOrLoad<Texture>(img.texture_handle);
      if (texture) {
        renderer->DrawTexturedRect(rt.computed_position, rt.computed_size,
                                   texture, img.tint, img.uv_rect,
                                   textured_layout, eid);
      }
      break;
    }
    case CanvasElementType::Button: {
      auto& btn = scene->GetComponent<ButtonComponent>(entry.entity);
      auto& rt = scene->GetComponent<RectangleTransformComponent>(entry.entity);
      auto [tex_handle, tint] = GetButtonVisuals(btn);
      auto texture = Engine::asset_manager().GetOrLoad<Texture>(tex_handle);
      if (texture) {
        renderer->DrawTexturedRect(rt.computed_position, rt.computed_size,
                                   texture, tint, {0, 0, 1, 1}, textured_layout,
                                   eid);
      }
      break;
    }
    case CanvasElementType::Text: {
      auto& text = scene->GetComponent<TextComponent>(entry.entity);
      auto& rt = scene->GetComponent<RectangleTransformComponent>(entry.entity);
      renderer->DrawCanvasText(rt, text, textured_layout, eid);
      break;
    }
    case CanvasElementType::UIDocument: {
      auto& doc = scene->GetComponent<UIDocumentComponent>(entry.entity);
      auto& rt = scene->GetComponent<RectangleTransformComponent>(entry.entity);
      if (doc.offscreen_descriptor_ && doc.visible) {
        renderer->DrawCanvasDescriptor(rt.computed_position, rt.computed_size,
                                       doc.offscreen_descriptor_,
                                       textured_layout, eid);
      }
      break;
    }
  }
}

CanvasFeature::CanvasFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Render pass: RGBA color + R32F entity ID, no MSAA
  render_pass_ =
      std::make_shared<RenderPass>(PassType::PostProcess, "Canvas RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = VK_FORMAT_R32_UINT,
                              .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  // Descriptor layouts
  canvas_element_layout_ = std::make_shared<DescriptorSetLayout>();
  canvas_element_layout_->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     VK_SHADER_STAGE_VERTEX_BIT);
  canvas_element_layout_->Bake();

  canvas_textured_layout_ = std::make_shared<DescriptorSetLayout>();
  canvas_textured_layout_->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_VERTEX_BIT);
  canvas_textured_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_FRAGMENT_BIT);
  canvas_textured_layout_->Bake();

  // Shaders
  auto canvas_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/canvas_shader.vert"});
  auto rect_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/canvas_rect.frag"});
  auto image_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/canvas_image.frag"});
  auto text_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/canvas_text.frag"});

  // Push constant for screen size
  screen_size_push_ = std::make_shared<CanvasScreenPushConstant>();

  // Pipeline properties: alpha blending, no depth, no culling, no vertex input
  PipelineProperties props{
      SamplingMode::DISABLED, CullModeNone, false, true, false, false};

  // Rect pipeline (UBO only)
  rect_pipeline_ = std::make_shared<Pipeline>(props);
  rect_pipeline_->SetRenderPass(render_pass_);
  rect_pipeline_->AddInputLayout(canvas_element_layout_);
  rect_pipeline_->AddShader(canvas_vert);
  rect_pipeline_->AddShader(rect_frag);
  rect_pipeline_->AddPushConstant(screen_size_push_,
                                  VK_SHADER_STAGE_VERTEX_BIT);
  rect_pipeline_->Bake();

  // Image pipeline (UBO + texture sampler)
  image_pipeline_ = std::make_shared<Pipeline>(props);
  image_pipeline_->SetRenderPass(render_pass_);
  image_pipeline_->AddInputLayout(canvas_textured_layout_);
  image_pipeline_->AddShader(canvas_vert);
  image_pipeline_->AddShader(image_frag);
  image_pipeline_->AddPushConstant(screen_size_push_,
                                   VK_SHADER_STAGE_VERTEX_BIT);
  image_pipeline_->Bake();

  // Text pipeline (UBO + font atlas sampler)
  text_pipeline_ = std::make_shared<Pipeline>(props);
  text_pipeline_->SetRenderPass(render_pass_);
  text_pipeline_->AddInputLayout(canvas_textured_layout_);
  text_pipeline_->AddShader(canvas_vert);
  text_pipeline_->AddShader(text_frag);
  text_pipeline_->AddPushConstant(screen_size_push_,
                                  VK_SHADER_STAGE_VERTEX_BIT);
  text_pipeline_->Bake();

  // World-space canvas pipelines (for WorldSpace and ScreenSpaceCamera modes)
  auto canvas_world_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/canvas_world.vert"});

  world_push_ = std::make_shared<CanvasWorldPushConstant>();

  world_render_pass_ = std::make_shared<RenderPass>(PassType::PostProcess,
                                                    "CanvasWorld RenderPass");
  world_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = renderer_->GetSwapChainImageFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  world_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                    .format = VK_FORMAT_R32_UINT,
                                    .msaa_mode = SamplingMode::DISABLED});
  world_render_pass_->Bake();

  // World rect pipeline
  world_rect_pipeline_ = std::make_shared<Pipeline>(props);
  world_rect_pipeline_->SetRenderPass(world_render_pass_);
  world_rect_pipeline_->AddInputLayout(canvas_element_layout_);
  world_rect_pipeline_->AddInputLayout(
      renderer_->GetDescriptorLayout("Global"));
  world_rect_pipeline_->AddShader(canvas_world_vert);
  world_rect_pipeline_->AddShader(rect_frag);
  world_rect_pipeline_->AddPushConstant(world_push_,
                                        VK_SHADER_STAGE_VERTEX_BIT);
  world_rect_pipeline_->Bake();

  // World image pipeline
  world_image_pipeline_ = std::make_shared<Pipeline>(props);
  world_image_pipeline_->SetRenderPass(world_render_pass_);
  world_image_pipeline_->AddInputLayout(canvas_textured_layout_);
  world_image_pipeline_->AddInputLayout(
      renderer_->GetDescriptorLayout("Global"));
  world_image_pipeline_->AddShader(canvas_world_vert);
  world_image_pipeline_->AddShader(image_frag);
  world_image_pipeline_->AddPushConstant(world_push_,
                                         VK_SHADER_STAGE_VERTEX_BIT);
  world_image_pipeline_->Bake();

  // World text pipeline
  world_text_pipeline_ = std::make_shared<Pipeline>(props);
  world_text_pipeline_->SetRenderPass(world_render_pass_);
  world_text_pipeline_->AddInputLayout(canvas_textured_layout_);
  world_text_pipeline_->AddInputLayout(
      renderer_->GetDescriptorLayout("Global"));
  world_text_pipeline_->AddShader(canvas_world_vert);
  world_text_pipeline_->AddShader(text_frag);
  world_text_pipeline_->AddPushConstant(world_push_,
                                        VK_SHADER_STAGE_VERTEX_BIT);
  world_text_pipeline_->Bake();

  // Composite pass: blends canvas offscreen onto PipelineOutput
  comp_render_pass_ = std::make_shared<RenderPass>(
      PassType::PostProcess, "CanvasComposite RenderPass");
  comp_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = renderer_->GetSwapChainImageFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  comp_render_pass_->Bake();

  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  auto quad_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/quad_shader.frag"});
  comp_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, true, true, false});
  comp_pipeline_->SetRenderPass(comp_render_pass_);
  comp_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Present"));
  comp_pipeline_->AddShader(fullscreen_vert);
  comp_pipeline_->AddShader(quad_frag);
  comp_pipeline_->Bake();

  // Get the RmlUi render pass from UIManager's render interface
  rmlui_render_pass_ =
      Engine::ui_manager().GetRenderInterface()->GetRenderPass();
}

void CanvasFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("CanvasFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  // Overlay canvas offscreen
  pool.SetTexture("canvas.color", renderer.CreateAttachmentTexture(
                                      {rw, rh, AttachmentTextureType::Offscreen,
                                       1, renderer.GetSwapChainImageFormat(),
                                       SamplingMode::DISABLED, true}));
  pool.SetTexture("canvas.entity_id",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R32_UINT, SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 2> attachments{
      pool.GetTexture("canvas.color").get(),
      pool.GetTexture("canvas.entity_id").get()};
  pool.SetFramebuffer(
      "canvas", render_pass_->CreateFramebuffer(0, attachments, {rw, rh}));

  auto canvas_output_desc = std::make_shared<DescriptorSet>();
  canvas_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  canvas_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("canvas.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  canvas_output_desc->Bake();
  pool.SetDescriptor("canvas.output", canvas_output_desc);

  // World-space canvas offscreen
  pool.SetTexture(
      "canvas_world.color",
      renderer.CreateAttachmentTexture(
          {rw, rh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));
  pool.SetTexture("canvas_world.entity_id",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R32_UINT, SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 2> world_attachments{
      pool.GetTexture("canvas_world.color").get(),
      pool.GetTexture("canvas_world.entity_id").get()};
  pool.SetFramebuffer("canvas_world", world_render_pass_->CreateFramebuffer(
                                          0, world_attachments, {rw, rh}));

  // Descriptor for compositing world canvas
  auto world_canvas_desc = std::make_shared<DescriptorSet>();
  world_canvas_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  world_canvas_desc->AddCombinedImageSampler(
      0, pool.GetTexture("canvas_world.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  world_canvas_desc->Bake();
  pool.SetDescriptor("canvas_world.output", world_canvas_desc);

  // Per-canvas camera quad offscreen (for ScreenSpaceCamera / external overlay)
  pool.SetTexture(
      "canvas_camera.color",
      renderer.CreateAttachmentTexture(
          {rw, rh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));
  pool.SetTexture("canvas_camera.entity_id",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       VK_FORMAT_R32_UINT, SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 2> camera_attachments{
      pool.GetTexture("canvas_camera.color").get(),
      pool.GetTexture("canvas_camera.entity_id").get()};
  pool.SetFramebuffer("canvas_camera", world_render_pass_->CreateFramebuffer(
                                           0, camera_attachments, {rw, rh}));

  auto camera_canvas_desc = std::make_shared<DescriptorSet>();
  camera_canvas_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  camera_canvas_desc->AddCombinedImageSampler(
      0, pool.GetTexture("canvas_camera.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  camera_canvas_desc->Bake();
  pool.SetDescriptor("canvas_camera.output", camera_canvas_desc);

  // Canvas composite: blend canvas onto PipelineOutput
  pool.SetTexture(
      "canvas_comp.color",
      renderer.CreateAttachmentTexture(
          {rw, rh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 1> comp_attachments{
      pool.GetTexture("canvas_comp.color").get()};
  pool.SetFramebuffer("canvas_comp", comp_render_pass_->CreateFramebuffer(
                                         0, comp_attachments, {rw, rh}));

  // Descriptor to read previous PipelineOutput
  auto comp_input_desc = std::make_shared<DescriptorSet>();
  comp_input_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  comp_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  comp_input_desc->Bake();
  pool.SetDescriptor("canvas_comp.input", comp_input_desc);

  // Update PipelineOutput for downstream features
  auto comp_output_desc = std::make_shared<DescriptorSet>();
  comp_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  comp_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("canvas_comp.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  comp_output_desc->Bake();
  pool.SetTexture("PipelineOutput", pool.GetTexture("canvas_comp.color"));
  pool.SetDescriptor("PipelineOutputDescriptor", comp_output_desc);

  // Clear per-canvas resources on resize (they will be recreated lazily)
  per_canvas_resources_.clear();
}

void CanvasFeature::AddPasses(RenderGraph& graph,
                              RenderResourceRegistry& registry,
                              RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("CanvasFeature::AddPasses");
  CameraResourcePool* pool = &ctx.resources;
  std::shared_ptr<Renderer> renderer = renderer_;
  std::shared_ptr<Pipeline> rect_pipeline = rect_pipeline_;
  std::shared_ptr<Pipeline> image_pipeline = image_pipeline_;
  std::shared_ptr<Pipeline> text_pipeline = text_pipeline_;
  std::shared_ptr<CanvasScreenPushConstant> screen_push = screen_size_push_;
  std::shared_ptr<DescriptorSetLayout> element_layout = canvas_element_layout_;
  std::shared_ptr<DescriptorSetLayout> textured_layout =
      canvas_textured_layout_;

  std::shared_ptr<Pipeline> world_rect_pipeline = world_rect_pipeline_;
  std::shared_ptr<Pipeline> world_image_pipeline = world_image_pipeline_;
  std::shared_ptr<Pipeline> world_text_pipeline = world_text_pipeline_;
  std::shared_ptr<CanvasWorldPushConstant> world_push = world_push_;

  // Compute effective screen size for canvas rendering.
  // If any canvas has a scaler, use its reference resolution.
  glm::vec2 effective_screen = ctx.viewport_size;
  bool found_scaler = false;
  ctx.scenes.ForEach<CanvasComponent, CanvasScalerComponent>(
      [&](Scene& scene, entt::entity e) {
        if (found_scaler) {
          return;
        }
        auto& scaler = scene.GetComponent<CanvasScalerComponent>(e);
        if (scaler.scale_mode == ScaleMode::ScaleWithScreenSize) {
          float scale_w = ctx.viewport_size.x / scaler.reference_resolution.x;
          float scale_h = ctx.viewport_size.y / scaler.reference_resolution.y;
          float t = scaler.match_width_or_height;
          float scale_factor = scale_w * (1.0f - t) + scale_h * t;
          effective_screen = ctx.viewport_size / scale_factor;
          found_scaler = true;
        }
      });
  auto viewport = effective_screen;

  // Pre-process text: load fonts, rasterize any new glyphs, and upload
  // atlases BEFORE command recording begins.  GPU resource creation
  // (texture upload) is invalid inside a render pass.
  //
  // Phase 1: Touch all codepoints across all text entities so any
  // on-demand rasterization happens now.  Collect unique fonts.
  std::unordered_map<Font*, std::shared_ptr<Font>> unique_fonts;
  ctx.scenes.ForEach<TextComponent, RectangleTransformComponent>(
      [&](Scene& scene, entt::entity entity) {
        auto& text = scene.GetComponent<TextComponent>(entity);
        if (text.text.empty()) {
          return;
        }
        std::shared_ptr<Font> font =
            FontCache::Get(text.font_handle, text.font_size);
        if (!font || !font->IsLoaded()) {
          return;
        }
        for (size_t i = 0; i < text.text.size();) {
          uint32_t cp = Font::DecodeUTF8(text.text, i);
          font->GetGlyph(cp);
        }
        unique_fonts[font.get()] = font;
      });

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
    ctx.scenes.ForEach<TextComponent, RectangleTransformComponent>(
        [&](Scene& scene, entt::entity entity) {
          auto& text = scene.GetComponent<TextComponent>(entity);
          std::shared_ptr<Font> font =
              FontCache::Get(text.font_handle, text.font_size);
          if (font && flushed_fonts.contains(font.get())) {
            text.glyph_gpu_.clear();
          }
        });
  }

  // Build a cache from (scene, entity) -> canvas root (nearest ancestor with CanvasComponent)
  std::unordered_map<Entity, entt::entity> canvas_root_cache;

  auto find_canvas_root =
      [&canvas_root_cache](Scene& scene, entt::entity entity) -> entt::entity {
    Entity key{entity, &scene};
    // Check cache first
    auto cache_it = canvas_root_cache.find(key);
    if (cache_it != canvas_root_cache.end()) {
      return cache_it->second;
    }

    // Walk up the tree to find the nearest CanvasComponent ancestor
    entt::entity current = entity;
    while (current != entt::null) {
      if (scene.HasComponent<CanvasComponent>(current)) {
        canvas_root_cache[key] = current;
        return current;
      }
      if (!scene.HasComponent<TreeComponent>(current)) {
        break;
      }
      auto& tree = scene.GetComponent<TreeComponent>(current);
      current = tree.parent;
    }

    // No canvas root found
    canvas_root_cache[key] = entt::null;
    return entt::null;
  };

  bool is_external = ctx.is_external;

  // Three categories:
  //   overlay_list - rendered to shared overlay offscreen (screen-space, game view only)
  //   world_list   - rendered per-element as 3D geometry (WorldSpace canvases)
  //   camera_list  - rendered to per-canvas offscreen texture, then drawn as 3D quad
  //                  (ScreenSpaceCamera canvases, and overlay canvases when is_external)
  std::vector<CanvasDrawEntry> overlay_list;
  std::vector<CanvasDrawEntry> world_list;
  std::vector<CanvasDrawEntry> camera_list;

  entt::entity current_camera_entity = ctx.camera_entity;

  auto classify_entry = [&find_canvas_root, &overlay_list, &world_list,
                         &camera_list, is_external, current_camera_entity](
                            Scene& scene, entt::entity entity,
                            CanvasElementType type, int32_t draw_order) {
    entt::entity canvas_root = find_canvas_root(scene, entity);
    if (canvas_root == entt::null) {
      overlay_list.push_back({&scene, entity, entt::null, type, draw_order});
      return;
    }

    auto& canvas = scene.GetComponent<CanvasComponent>(canvas_root);
    if (canvas.render_mode == CanvasRenderMode::ScreenSpaceOverlay) {
      if (is_external) {
        camera_list.push_back({&scene, entity, canvas_root, type, draw_order});
      } else {
        overlay_list.push_back({&scene, entity, canvas_root, type, draw_order});
      }
    } else if (canvas.render_mode == CanvasRenderMode::ScreenSpaceCamera) {
      if (is_external) {
        // Editor: show as 3D quad at entity's Transform position
        camera_list.push_back({&scene, entity, canvas_root, type, draw_order});
      } else if (canvas.camera_entity == current_camera_entity) {
        // Game: render as screen-space overlay for the assigned camera
        overlay_list.push_back({&scene, entity, canvas_root, type, draw_order});
      }
    } else {
      world_list.push_back({&scene, entity, canvas_root, type, draw_order});
    }
  };

  ctx.scenes.ForEach<CanvasRectComponent, RectangleTransformComponent>(
      [&](Scene& scene, entt::entity entity) {
        auto& rt = scene.GetComponent<RectangleTransformComponent>(entity);
        classify_entry(scene, entity, CanvasElementType::Rect, rt.draw_order);
      });
  ctx.scenes.ForEach<CanvasImageComponent, RectangleTransformComponent>(
      [&](Scene& scene, entt::entity entity) {
        auto& rt = scene.GetComponent<RectangleTransformComponent>(entity);
        classify_entry(scene, entity, CanvasElementType::Image, rt.draw_order);
      });
  ctx.scenes.ForEach<ButtonComponent, RectangleTransformComponent>(
      [&](Scene& scene, entt::entity entity) {
        auto& rt = scene.GetComponent<RectangleTransformComponent>(entity);
        classify_entry(scene, entity, CanvasElementType::Button, rt.draw_order);
      });
  ctx.scenes.ForEach<TextComponent, RectangleTransformComponent>(
      [&](Scene& scene, entt::entity entity) {
        auto& rt = scene.GetComponent<RectangleTransformComponent>(entity);
        classify_entry(scene, entity, CanvasElementType::Text, rt.draw_order);
      });
  ctx.scenes.ForEach<UIDocumentComponent, RectangleTransformComponent>(
      [&](Scene& scene, entt::entity entity) {
        auto& rt = scene.GetComponent<RectangleTransformComponent>(entity);
        classify_entry(scene, entity, CanvasElementType::UIDocument,
                       rt.draw_order);
      });

  std::ranges::sort(overlay_list,
                    [](const CanvasDrawEntry& a, const CanvasDrawEntry& b) {
                      return a.draw_order < b.draw_order;
                    });
  std::ranges::sort(world_list,
                    [](const CanvasDrawEntry& a, const CanvasDrawEntry& b) {
                      return a.draw_order < b.draw_order;
                    });
  std::ranges::sort(camera_list,
                    [](const CanvasDrawEntry& a, const CanvasDrawEntry& b) {
                      return a.draw_order < b.draw_order;
                    });

  // Pre-render RmlUi documents to offscreen textures.
  // This must happen before render passes since it creates GPU resources.
  {
    PROFILE_ZONE_SCOPED_N("CanvasFeature::RmlUiPreRender");
    Renderer& renderer = *renderer_;
    auto rml_render_pass = rmlui_render_pass_;
    ctx.scenes.ForEach<UIDocumentComponent,
                       RectangleTransformComponent>([&](Scene& scene,
                                                        entt::entity entity) {
      auto& doc = scene.GetComponent<UIDocumentComponent>(entity);
      auto& rt = scene.GetComponent<RectangleTransformComponent>(entity);
      if (!doc.rml_context_ || !doc.rml_document_ || !doc.visible) {
        return;
      }

      glm::vec2 size = rt.computed_size;
      if (size.x < 1 || size.y < 1) {
        return;
      }

      uint32_t w = static_cast<uint32_t>(size.x);
      uint32_t h = static_cast<uint32_t>(size.y);

      // Render at actual viewport resolution for crisp output, but use
      // DPI ratio so dp-based layouts match the canvas scaler's reference.
      float dpi_ratio = 1.0f;
      if (size.x > 0 && effective_screen.x > 0 && ctx.viewport_size.x > 0 &&
          ctx.viewport_size.y > 0 && ctx.viewport_size.x < 16384 &&
          ctx.viewport_size.y < 16384) {
        dpi_ratio = ctx.viewport_size.x / effective_screen.x;
      }

      // Use full viewport-scaled size for the offscreen texture
      uint32_t render_w = static_cast<uint32_t>(size.x * dpi_ratio);
      uint32_t render_h = static_cast<uint32_t>(size.y * dpi_ratio);
      if (render_w < 1) {
        render_w = w;
      }
      if (render_h < 1) {
        render_h = h;
      }

      // Recreate offscreen at render resolution if needed
      glm::vec2 render_size{render_w, render_h};
      if (!doc.offscreen_texture_ || doc.offscreen_size_ != render_size) {
        doc.offscreen_texture_ = renderer.CreateAttachmentTexture(
            {render_w, render_h, AttachmentTextureType::Offscreen, 1,
             renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true});
        doc.offscreen_stencil_ = renderer.CreateAttachmentTexture(
            {render_w, render_h, AttachmentTextureType::DepthStencil, 1,
             renderer.FindDepthStencilFormat(), SamplingMode::DISABLED, false});

        std::array<AttachmentTexture*, 2> att{doc.offscreen_texture_.get(),
                                              doc.offscreen_stencil_.get()};
        doc.offscreen_framebuffer_ =
            rml_render_pass->CreateFramebuffer(0, att, {render_w, render_h});

        // Rebuild descriptor with new texture
        doc.offscreen_ubo_ = renderer.CreateUniformBuffer(
            "CanvasFeature offscreen_ubo_", sizeof(CanvasElementUniformData));
        doc.offscreen_descriptor_ = std::make_shared<DescriptorSet>();
        doc.offscreen_descriptor_->SetLayout(canvas_textured_layout_);
        doc.offscreen_descriptor_->AddUniformBuffer(0, doc.offscreen_ubo_);
        doc.offscreen_descriptor_->AddCombinedImageSampler(
            1, doc.offscreen_texture_->image_views_[0],
            renderer.GetDefaultLinearSampler());
        doc.offscreen_descriptor_->Bake();

        doc.offscreen_size_ = render_size;
      }

      doc.rml_context_->SetDimensions(Rml::Vector2i(
          static_cast<int>(render_w), static_cast<int>(render_h)));
      doc.rml_context_->SetDensityIndependentPixelRatio(dpi_ratio);
      doc.data_model.Flush();
      doc.rml_context_->Update();

      // Update UBO with canvas element position/size for drawing
      if (doc.offscreen_ubo_) {
        CanvasElementUniformData ubo_data{};
        ubo_data.position = rt.computed_position;
        ubo_data.size = rt.computed_size;
        ubo_data.color = {1, 1, 1, 1};
        ubo_data.uv_rect = {0, 0, 1, 1};
        ubo_data.entity_id =
            (static_cast<uint32_t>(renderer_->GetCurrentSceneIndex()) << 24) |
            (static_cast<uint32_t>(entity) + 1);
        ubo_data.premultiplied = 1.0f;
        memcpy(doc.offscreen_ubo_->data_, &ubo_data,
               sizeof(CanvasElementUniformData));
      }
    });
  }

  auto sorted_overlay =
      std::make_shared<std::vector<CanvasDrawEntry>>(std::move(overlay_list));
  auto sorted_world =
      std::make_shared<std::vector<CanvasDrawEntry>>(std::move(world_list));
  auto sorted_camera =
      std::make_shared<std::vector<CanvasDrawEntry>>(std::move(camera_list));

  // ---- Unified per-canvas info computation ----
  // Compute ALL per-canvas info once. Every consumer references this single map.
  struct CanvasRenderInfo {
    Entity entity{entt::null, nullptr};
    CanvasRenderMode render_mode;
    glm::mat4 model_matrix;
    glm::vec2 canvas_size;  // reference resolution for layout/shader
    glm::vec2 world_size;   // auto-computed with aspect correction
    glm::vec2
        texture_size;  // offscreen texture resolution (for camera/overlay-in-editor)
    float plane_distance;
    std::shared_ptr<PerCanvasResources> per_canvas_res;
  };

  // Collect ALL unique canvas roots from all three lists
  std::unordered_set<Entity> all_canvas_roots;
  for (const CanvasDrawEntry& e : *sorted_overlay) {
    if (e.canvas_root != entt::null) {
      all_canvas_roots.insert(Entity{e.canvas_root, e.scene});
    }
  }
  for (const CanvasDrawEntry& e : *sorted_world) {
    if (e.canvas_root != entt::null) {
      all_canvas_roots.insert(Entity{e.canvas_root, e.scene});
    }
  }
  for (const CanvasDrawEntry& e : *sorted_camera) {
    if (e.canvas_root != entt::null) {
      all_canvas_roots.insert(Entity{e.canvas_root, e.scene});
    }
  }

  // Also include all CanvasComponent entities for border drawing in editor
  if (is_external) {
    ctx.scenes.ForEach<CanvasComponent>([&](Scene& scene, entt::entity ce) {
      all_canvas_roots.insert(Entity{ce, &scene});
    });
  }

  auto canvas_infos =
      std::make_shared<std::unordered_map<Entity, CanvasRenderInfo>>();

  // Ordered list of camera canvas roots (preserving first-seen order from sorted_camera)
  std::vector<Entity> camera_canvas_roots;
  {
    std::unordered_set<Entity> seen;
    for (const auto& entry : *sorted_camera) {
      Entity key{entry.canvas_root, entry.scene};
      if (entry.canvas_root != entt::null && !seen.contains(key)) {
        seen.insert(key);
        camera_canvas_roots.push_back(key);
      }
    }
  }

  std::shared_ptr<CameraData> camera_data = renderer_->GetCameraData();

  for (Entity entity : all_canvas_roots) {
    Scene& root_scene = *entity.GetScene();
    auto& canvas = entity.GetComponent<CanvasComponent>();
    auto& canvas_rt = entity.GetComponent<RectangleTransformComponent>();

    CanvasRenderInfo info{};
    info.entity = entity;
    info.render_mode = canvas.render_mode;
    info.plane_distance = canvas.plane_distance;

    // canvas_size: scaler reference_resolution if available, else RectTransform computed_size
    if (entity.HasComponent<CanvasScalerComponent>()) {
      auto& scaler = entity.GetComponent<CanvasScalerComponent>();
      if (scaler.scale_mode == ScaleMode::ScaleWithScreenSize) {
        info.canvas_size = scaler.reference_resolution;
      } else {
        info.canvas_size = canvas_rt.computed_size;
      }
    } else {
      info.canvas_size = canvas_rt.computed_size;
    }

    // world_size: derived from canvas_size and reference_pixels_per_unit
    if (info.render_mode == CanvasRenderMode::WorldSpace) {
      float ppu = 100.0f;
      if (entity.HasComponent<CanvasScalerComponent>()) {
        ppu = std::max(1.0f, entity.GetComponent<CanvasScalerComponent>()
                                 .reference_pixels_per_unit);
      }
      info.world_size = info.canvas_size / ppu;
    } else {
      // For overlay/camera in editor: use a fixed 5.0 height for preview
      float aspect = info.canvas_size.x / std::max(1.0f, info.canvas_size.y);
      info.world_size = {5.0f * aspect, 5.0f};
    }

    // texture_size: for offscreen rendering of camera/overlay-in-editor canvases.
    // Use scaler reference resolution if available, otherwise viewport.
    if (info.render_mode != CanvasRenderMode::WorldSpace) {
      if (entity.HasComponent<CanvasScalerComponent>()) {
        auto& scaler = entity.GetComponent<CanvasScalerComponent>();
        if (scaler.scale_mode == ScaleMode::ScaleWithScreenSize) {
          info.texture_size = scaler.reference_resolution;
        } else {
          info.texture_size = ctx.viewport_size;
        }
      } else {
        info.texture_size = ctx.viewport_size;
      }
    }

    // model_matrix depends on mode + is_external
    if (is_external || info.render_mode == CanvasRenderMode::WorldSpace) {
      // Editor: all modes use entity Transform
      // Game WorldSpace: uses entity Transform
      if (entity.HasComponent<TransformComponent>()) {
        info.model_matrix =
            entity.GetComponent<TransformComponent>().GetTransformMatrix();
      } else {
        info.model_matrix = glm::mat4(1.0f);
      }
    } else if (info.render_mode == CanvasRenderMode::ScreenSpaceCamera &&
               !is_external) {
      // Game ScreenSpaceCamera: billboard in front of assigned camera
      if (canvas.camera_entity != entt::null &&
          root_scene.HasComponent<CameraComponent>(canvas.camera_entity) &&
          root_scene.HasComponent<TransformComponent>(canvas.camera_entity)) {
        auto& cam =
            root_scene.GetComponent<CameraComponent>(canvas.camera_entity);
        auto& cam_transform =
            root_scene.GetComponent<TransformComponent>(canvas.camera_entity);

        float fov_rad = glm::radians(cam.field_of_view * 0.5f);
        float tex_aspect =
            info.texture_size.x / std::max(1.0f, info.texture_size.y);
        float world_h = 2.0f * canvas.plane_distance * std::tan(fov_rad);
        float world_w = world_h * tex_aspect;
        info.world_size = {world_w, world_h};

        glm::mat4 cam_world = cam_transform.GetTransformMatrix();
        glm::vec3 cam_pos = glm::vec3(cam_world[3]);
        glm::vec3 cam_right = glm::normalize(glm::vec3(cam_world[0]));
        glm::vec3 cam_up = glm::normalize(glm::vec3(cam_world[1]));
        glm::vec3 cam_forward = glm::normalize(glm::vec3(cam_world[2]));
        glm::vec3 center = cam_pos + cam_forward * canvas.plane_distance;

        info.model_matrix = glm::mat4(1.0f);
        info.model_matrix[0] = glm::vec4(cam_right, 0.0f);
        info.model_matrix[1] = glm::vec4(cam_up, 0.0f);
        info.model_matrix[2] = glm::vec4(-cam_forward, 0.0f);
        info.model_matrix[3] = glm::vec4(center, 1.0f);
      } else {
        info.model_matrix = glm::mat4(1.0f);
      }
    } else {
      info.model_matrix = glm::mat4(1.0f);
    }

    // Per-canvas offscreen resources (for camera_list canvases)
    bool needs_offscreen =
        (is_external && info.render_mode != CanvasRenderMode::WorldSpace) ||
        (!is_external &&
         info.render_mode == CanvasRenderMode::ScreenSpaceCamera);
    if (needs_offscreen) {
      uint32_t tw = static_cast<uint32_t>(std::max(1.0f, info.texture_size.x));
      uint32_t th = static_cast<uint32_t>(std::max(1.0f, info.texture_size.y));

      auto& res = per_canvas_resources_[entity];
      if (!res.texture || res.width != tw || res.height != th) {
        res.texture = renderer_->CreateAttachmentTexture(
            {tw, th, AttachmentTextureType::Offscreen, 1,
             renderer_->GetSwapChainImageFormat(), SamplingMode::DISABLED,
             true});
        res.entity_id_texture = renderer_->CreateAttachmentTexture(
            {tw, th, AttachmentTextureType::Offscreen, 1, VK_FORMAT_R32_UINT,
             SamplingMode::DISABLED, true});

        std::array<AttachmentTexture*, 2> att{res.texture.get(),
                                              res.entity_id_texture.get()};
        res.framebuffer = render_pass_->CreateFramebuffer(0, att, {tw, th});

        res.output_descriptor = std::make_shared<DescriptorSet>();
        res.output_descriptor->SetLayout(
            renderer_->GetDescriptorLayout("Present"));
        res.output_descriptor->AddCombinedImageSampler(
            0, res.texture->image_views_[0],
            renderer_->GetDefaultLinearSampler());
        res.output_descriptor->Bake();

        res.width = tw;
        res.height = th;
      }

      info.per_canvas_res = std::make_shared<PerCanvasResources>(res);
    }

    (*canvas_infos)[entity] = info;
  }

  // Get camera global descriptor for world-space rendering
  std::shared_ptr<DescriptorSet> global_descriptor;
  if (!sorted_world->empty() || !sorted_camera->empty()) {
    if (pool->HasDescriptor("GlobalDescriptor")) {
      global_descriptor = pool->GetDescriptor("GlobalDescriptor");
    }
  }

  // ---- World-space canvas pass (renders BEFORE overlay) ----
  RGResource canvas_world_out = graph.ImportTexture(
      "CanvasWorldOut", pool->GetTexture("canvas_world.color"));

  // Build canvas border data for editor scene view
  struct CanvasBorderInfo {
    Entity canvas_key;
    std::shared_ptr<UniformBuffer> edge_ubos[4];
    std::shared_ptr<DescriptorSet> edge_descriptors[4];
  };

  auto canvas_borders = std::make_shared<std::vector<CanvasBorderInfo>>();

  if (is_external) {
    ctx.scenes.ForEach<CanvasComponent>([&](Scene& scene,
                                            entt::entity canvas_entity) {
      Entity key{canvas_entity, &scene};
      auto info_it = canvas_infos->find(key);
      if (info_it == canvas_infos->end()) {
        return;
      }
      const CanvasRenderInfo& ci = info_it->second;

      float bw = 2.0f;

      struct EdgeRect {
        glm::vec2 pos;
        glm::vec2 size;
      };

      EdgeRect edges[4] = {
          {{0, 0}, {ci.canvas_size.x, bw}},                      // top
          {{0, ci.canvas_size.y - bw}, {ci.canvas_size.x, bw}},  // bottom
          {{0, 0}, {bw, ci.canvas_size.y}},                      // left
          {{ci.canvas_size.x - bw, 0}, {bw, ci.canvas_size.y}},  // right
      };

      CanvasBorderInfo border{key, {}, {}};

      for (int ei = 0; ei < 4; ei++) {
        border.edge_ubos[ei] = renderer_->CreateUniformBuffer(
            "CanvasFeature edge_ubos", sizeof(CanvasElementUniformData));
        CanvasElementUniformData data{};
        data.position = edges[ei].pos;
        data.size = edges[ei].size;
        data.color = {1.0f, 1.0f, 1.0f, 0.5f};
        data.uv_rect = {0, 0, 1, 1};
        memcpy(border.edge_ubos[ei]->data_, &data,
               sizeof(CanvasElementUniformData));

        border.edge_descriptors[ei] = std::make_shared<DescriptorSet>();
        border.edge_descriptors[ei]->SetLayout(canvas_element_layout_);
        border.edge_descriptors[ei]->AddUniformBuffer(0, border.edge_ubos[ei]);
        border.edge_descriptors[ei]->Bake();
      }

      canvas_borders->push_back(std::move(border));
    });
  }

  // Pre-build quad draw data for camera canvases that need to render
  // as 3D quads in the world pass (editor scene view + ScreenSpaceCamera).
  struct CameraQuadDraw {
    Entity canvas_key;
    std::shared_ptr<UniformBuffer> ubo;
    std::shared_ptr<DescriptorSet> descriptor;
  };

  auto camera_quad_draws = std::make_shared<std::vector<CameraQuadDraw>>();

  // Render RmlUi documents to their offscreen textures
  std::vector<RGResource> rml_offscreen_textures;
  {
    auto rml_render_pass = rmlui_render_pass_;
    ctx.scenes.ForEach<UIDocumentComponent,
                       RectangleTransformComponent>([&](Scene& scene,
                                                        entt::entity entity) {
      auto& doc = scene.GetComponent<UIDocumentComponent>(entity);
      if (!doc.rml_context_ || !doc.rml_document_ || !doc.visible ||
          !doc.offscreen_framebuffer_) {
        return;
      }

      glm::vec2 doc_size = doc.offscreen_size_;
      auto fb = doc.offscreen_framebuffer_;
      Rml::Context* rml_ctx = doc.rml_context_;

      // Import offscreen texture into render graph for dependency tracking
      RGResource rml_tex = graph.ImportTexture(
          "RmlUiDoc_" + std::to_string(static_cast<uint32_t>(entity)),
          doc.offscreen_texture_);

      uint32_t rml_pass = graph.AddPass(
          "RmlUiOffscreen", rml_render_pass,
          [doc_size, rml_ctx](VkCommandBuffer cmd) {
            Engine::ui_manager().GetRenderInterface()->RenderToTexture(
                cmd, rml_ctx, doc_size);
          });
      graph.PassWritesColor(rml_pass, rml_tex);
      graph.SetPassFramebuffer(rml_pass, fb);
      graph.SetPassViewport(rml_pass, doc_size);
      graph.SetPassClearColor(rml_pass, {0, 0, 0, 0});

      rml_offscreen_textures.push_back(rml_tex);
    });
  }

  uint32_t world_canvas_pass = graph.AddPass(
      "CanvasWorld", world_render_pass_,
      [world_rect_pipeline, world_image_pipeline, world_text_pipeline,
       renderer, pool, world_push, element_layout, textured_layout,
       sorted_world, canvas_infos, global_descriptor, viewport,
       camera_quad_draws, canvas_borders](VkCommandBuffer) {
        if (sorted_world->empty() && camera_quad_draws->empty() &&
            canvas_borders->empty()) {
          return;
        }
        if (!global_descriptor) {
          return;
        }

        CanvasElementType bound_type = CanvasElementType::Rect;
        bool first = true;
        Scene* current_canvas_scene = nullptr;
        entt::entity current_canvas = entt::null;

        for (const auto& entry : *sorted_world) {
          // When the canvas root changes, update push constants
          if (entry.canvas_root != current_canvas ||
              entry.scene != current_canvas_scene) {
            current_canvas = entry.canvas_root;
            current_canvas_scene = entry.scene;

            auto info_it = canvas_infos->find(Entity{current_canvas, entry.scene});
            if (info_it == canvas_infos->end()) {
              continue;
            }
            const CanvasRenderInfo& ci = info_it->second;

            world_push->model_matrix = ci.model_matrix;
            world_push->canvas_size = ci.canvas_size;
            world_push->world_size = ci.world_size;

            // Force pipeline rebind to push new constants
            first = true;
          }

          // Rebind pipeline when element type changes or canvas changed
          if (first || entry.type != bound_type) {
            switch (entry.type) {
              case CanvasElementType::Rect:
                world_rect_pipeline->Bind(PipelineBindPointGraphics);
                break;
              case CanvasElementType::Image:
              case CanvasElementType::Button:
              case CanvasElementType::UIDocument:
                world_image_pipeline->Bind(PipelineBindPointGraphics);
                break;
              case CanvasElementType::Text:
                world_text_pipeline->Bind(PipelineBindPointGraphics);
                break;
            }
            bound_type = entry.type;
            first = false;

            // Bind the camera global descriptor at set 1
            VkDescriptorSet global_set = global_descriptor->descriptor_set_;
            auto* bound = renderer->GetBoundPipeline();
            if (bound) {
              vkCmdBindDescriptorSets(renderer->GetCommandBuffer().handle_,
                                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      bound->layout_, 1, 1, &global_set, 0,
                                      nullptr);
            }
          }

          uint32_t eid =
              (static_cast<uint32_t>(renderer->GetCurrentSceneIndex()) << 24) |
              (static_cast<uint32_t>(entry.entity) + 1);
          DrawCanvasElement(entry, renderer, element_layout,
                            textured_layout,
                            eid);
        }

        // Draw per-canvas textures as 3D quads (camera canvases in editor)
        for (const CameraQuadDraw& qd : *camera_quad_draws) {
          auto ci_it = canvas_infos->find(qd.canvas_key);
          if (ci_it == canvas_infos->end()) {
            continue;
          }
          const CanvasRenderInfo& ci = ci_it->second;

          world_push->model_matrix = ci.model_matrix;
          world_push->canvas_size = ci.canvas_size;
          world_push->world_size = ci.world_size;

          world_image_pipeline->Bind(PipelineBindPointGraphics);

          VkDescriptorSet global_set = global_descriptor->descriptor_set_;
          auto* bound = renderer->GetBoundPipeline();
          if (bound) {
            vkCmdBindDescriptorSets(renderer->GetCommandBuffer().handle_,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    bound->layout_, 1, 1, &global_set, 0,
                                    nullptr);
          }

          VkDescriptorSet sets[] = {qd.descriptor->descriptor_set_};
          vkCmdBindDescriptorSets(renderer->GetCommandBuffer().handle_,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  renderer->GetBoundPipeline()->layout_, 0, 1,
                                  sets, 0, nullptr);
          vkCmdDraw(renderer->GetCommandBuffer().handle_, 6, 1, 0, 0);
        }

        // Draw canvas borders in editor scene view
        for (const CanvasBorderInfo& border : *canvas_borders) {
          auto ci_it = canvas_infos->find(border.canvas_key);
          if (ci_it == canvas_infos->end()) {
            continue;
          }
          const CanvasRenderInfo& ci = ci_it->second;

          world_push->model_matrix = ci.model_matrix;
          world_push->canvas_size = ci.canvas_size;
          world_push->world_size = ci.world_size;

          world_rect_pipeline->Bind(PipelineBindPointGraphics);

          VkDescriptorSet global_set = global_descriptor->descriptor_set_;
          auto* bound = renderer->GetBoundPipeline();
          if (bound) {
            vkCmdBindDescriptorSets(renderer->GetCommandBuffer().handle_,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    bound->layout_, 1, 1, &global_set, 0,
                                    nullptr);
          }

          // Draw 4 thin rects as border edges - each needs its own UBO
          // since the GPU reads them later during execution.
          for (int ei = 0; ei < 4; ei++) {
            const std::shared_ptr<UniformBuffer>& edge_ubo =
                border.edge_ubos[ei];
            const std::shared_ptr<DescriptorSet>& edge_desc =
                border.edge_descriptors[ei];

            VkDescriptorSet sets[] = {edge_desc->descriptor_set_};
            vkCmdBindDescriptorSets(renderer->GetCommandBuffer().handle_,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    renderer->GetBoundPipeline()->layout_, 0, 1,
                                    sets, 0, nullptr);
            vkCmdDraw(renderer->GetCommandBuffer().handle_, 6, 1, 0, 0);
          }
        }
      });

  graph.PassWritesColor(world_canvas_pass, canvas_world_out);
  for (const auto& rml_tex : rml_offscreen_textures) {
    graph.PassReadsTexture(world_canvas_pass, rml_tex);
  }
  graph.SetPassFramebuffer(world_canvas_pass,
                           pool->GetFramebuffer("canvas_world"));
  graph.SetPassViewport(world_canvas_pass, ctx.viewport_size);
  graph.SetPassClearColor(world_canvas_pass, {0, 0, 0, 0});

  registry.Register("CanvasWorldOut", canvas_world_out);

  // ---- Per-canvas offscreen passes (render elements to per-canvas textures) ----
  // For each unique canvas root in camera_list, render its elements to its own
  // offscreen texture using the screen-space pipelines (same as overlay).
  struct PerCanvasPassInfo {
    Entity canvas_key;
    RGResource rg_resource;
  };

  auto per_canvas_pass_infos =
      std::make_shared<std::vector<PerCanvasPassInfo>>();

  for (const auto& canvas_key : camera_canvas_roots) {
    auto ci_it = canvas_infos->find(canvas_key);
    if (ci_it == canvas_infos->end()) {
      continue;
    }
    const CanvasRenderInfo& ci = ci_it->second;
    if (!ci.per_canvas_res || !ci.per_canvas_res->texture) {
      continue;
    }

    // Collect elements belonging to this canvas
    auto canvas_elements = std::make_shared<std::vector<CanvasDrawEntry>>();
    for (const auto& entry : *sorted_camera) {
      if (entry.scene == canvas_key.GetScene() &&
          entry.canvas_root == canvas_key.handle()) {
        canvas_elements->push_back(entry);
      }
    }

    if (canvas_elements->empty()) {
      continue;
    }

    glm::vec2 canvas_viewport = ci.texture_size;
    std::shared_ptr<PerCanvasResources> canvas_res = ci.per_canvas_res;
    std::string pass_name =
        "CanvasPerCanvas_" +
        std::to_string(static_cast<uint32_t>(canvas_key.handle()));

    RGResource canvas_tex_rg =
        graph.ImportTexture(pass_name, canvas_res->texture);

    uint32_t per_canvas_pass = graph.AddPass(
        pass_name, render_pass_,
        [rect_pipeline, image_pipeline, text_pipeline, renderer,
         screen_push, canvas_viewport, element_layout, textured_layout,
         canvas_elements](VkCommandBuffer) {
          screen_push->screen_size = canvas_viewport;

          CanvasElementType bound_type = CanvasElementType::Rect;
          bool first = true;

          for (const auto& entry : *canvas_elements) {
            if (first || entry.type != bound_type) {
              switch (entry.type) {
                case CanvasElementType::Rect:
                  rect_pipeline->Bind(PipelineBindPointGraphics);
                  break;
                case CanvasElementType::Image:
                case CanvasElementType::Button:
                case CanvasElementType::UIDocument:
                  image_pipeline->Bind(PipelineBindPointGraphics);
                  break;
                case CanvasElementType::Text:
                  text_pipeline->Bind(PipelineBindPointGraphics);
                  break;
              }
              bound_type = entry.type;
              first = false;
            }

            uint32_t eid =
                (static_cast<uint32_t>(renderer->GetCurrentSceneIndex()) << 24) |
                (static_cast<uint32_t>(entry.entity) + 1);
            DrawCanvasElement(entry, renderer, element_layout,
                              textured_layout, eid);
          }
        });

    graph.PassWritesColor(per_canvas_pass, canvas_tex_rg);
    for (const auto& rml_tex : rml_offscreen_textures) {
      graph.PassReadsTexture(per_canvas_pass, rml_tex);
    }
    graph.SetPassFramebuffer(per_canvas_pass, canvas_res->framebuffer);
    graph.SetPassViewport(per_canvas_pass, canvas_viewport);
    graph.SetPassClearColor(per_canvas_pass, {0, 0, 0, 0});

    per_canvas_pass_infos->push_back({canvas_key, canvas_tex_rg});
  }

  // ---- Per-canvas 3D quad pass ----
  // Draws each per-canvas offscreen texture as a textured quad in 3D,
  // rendered into a separate offscreen buffer that gets composited later.
  RGResource canvas_camera_out = graph.ImportTexture(
      "CanvasCameraOut", pool->GetTexture("canvas_camera.color"));
  bool camera_pass_added = !per_canvas_pass_infos->empty() && global_descriptor;

  // Pre-build per-canvas quad draw resources (UBO + descriptor) outside the
  // lambda so they are captured by shared_ptr and stay alive until the
  // render graph pass is destroyed (deferred via DeletionQueue).
  struct CanvasQuadDrawData {
    Entity canvas_key;
    std::shared_ptr<UniformBuffer> ubo;
    std::shared_ptr<DescriptorSet> descriptor;
  };

  auto quad_draw_datas = std::make_shared<std::vector<CanvasQuadDrawData>>();

  if (camera_pass_added) {
    for (const auto& pass_info : *per_canvas_pass_infos) {
      auto ci_it = canvas_infos->find(pass_info.canvas_key);
      if (ci_it == canvas_infos->end()) {
        continue;
      }
      const CanvasRenderInfo& ci = ci_it->second;
      if (!ci.per_canvas_res || !ci.per_canvas_res->texture) {
        continue;
      }

      auto ubo = renderer_->CreateUniformBuffer(
          "CanvasFeature canvas ubo", sizeof(CanvasElementUniformData));
      CanvasElementUniformData data{};
      data.position = {0.0f, 0.0f};
      data.size = ci.canvas_size;
      data.color = {1.0f, 1.0f, 1.0f, 1.0f};
      data.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
      memcpy(ubo->data_, &data, sizeof(CanvasElementUniformData));

      auto desc = std::make_shared<DescriptorSet>();
      desc->SetLayout(canvas_textured_layout_);
      desc->AddUniformBuffer(0, ubo);
      desc->AddCombinedImageSampler(1,
                                    ci.per_canvas_res->texture->image_views_[0],
                                    renderer_->GetDefaultLinearSampler());
      desc->Bake();

      quad_draw_datas->push_back({pass_info.canvas_key, ubo, desc});

      // When in editor scene view, also add to the world pass draw list
      if (is_external) {
        camera_quad_draws->push_back({pass_info.canvas_key, ubo, desc});
      }
    }
  }

  uint32_t camera_quad_pass = graph.AddPass(
      "CanvasCameraQuad", world_render_pass_,
      [world_image_pipeline, renderer, world_push, global_descriptor,
       camera_pass_added, canvas_infos, quad_draw_datas](VkCommandBuffer) {
        if (!camera_pass_added || !global_descriptor ||
            quad_draw_datas->empty()) {
          return;
        }

        world_image_pipeline->Bind(PipelineBindPointGraphics);

        // Bind the camera global descriptor at set 1
        VkDescriptorSet global_set = global_descriptor->descriptor_set_;
        auto* bound = renderer->GetBoundPipeline();
        if (bound) {
          vkCmdBindDescriptorSets(renderer->GetCommandBuffer().handle_,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  bound->layout_, 1, 1, &global_set, 0,
                                  nullptr);
        }

        for (const auto& qd : *quad_draw_datas) {
          auto ci_it = canvas_infos->find(qd.canvas_key);
          if (ci_it == canvas_infos->end()) {
            continue;
          }
          const auto& ci = ci_it->second;

          // Update push constants for this canvas quad
          world_push->model_matrix = ci.model_matrix;
          world_push->canvas_size = ci.canvas_size;
          world_push->world_size = ci.world_size;

          // Rebind pipeline to push new constants
          world_image_pipeline->Bind(PipelineBindPointGraphics);

          // Re-bind global descriptor after pipeline rebind
          bound = renderer->GetBoundPipeline();
          if (bound) {
            vkCmdBindDescriptorSets(renderer->GetCommandBuffer().handle_,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    bound->layout_, 1, 1, &global_set, 0,
                                    nullptr);
          }

          // Draw the full-canvas quad textured with the per-canvas offscreen
          VkDescriptorSet sets[] = {qd.descriptor->descriptor_set_};
          vkCmdBindDescriptorSets(renderer->GetCommandBuffer().handle_,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  renderer->GetBoundPipeline()->layout_, 0, 1,
                                  sets, 0, nullptr);
          vkCmdDraw(renderer->GetCommandBuffer().handle_, 6, 1, 0, 0);
        }
      });

  // The quad pass reads from each per-canvas texture
  for (const auto& pass_info : *per_canvas_pass_infos) {
    graph.PassReadsTexture(camera_quad_pass, pass_info.rg_resource);
  }
  graph.PassWritesColor(camera_quad_pass, canvas_camera_out);
  graph.SetPassFramebuffer(camera_quad_pass,
                           pool->GetFramebuffer("canvas_camera"));
  graph.SetPassViewport(camera_quad_pass, ctx.viewport_size);
  graph.SetPassClearColor(camera_quad_pass, {0, 0, 0, 0});

  // ---- Overlay canvas pass ----
  RGResource canvas_out =
      graph.ImportTexture("CanvasOut", pool->GetTexture("canvas.color"));

  uint32_t canvas_pass = graph.AddPass(
      "Canvas", render_pass_,
      [rect_pipeline, image_pipeline, text_pipeline, renderer, pool,
       screen_push, viewport, element_layout, textured_layout,
       sorted_overlay](VkCommandBuffer) {
        screen_push->screen_size = viewport;

        CanvasElementType bound_type = CanvasElementType::Rect;
        bool first = true;

        for (const auto& entry : *sorted_overlay) {
          if (first || entry.type != bound_type) {
            switch (entry.type) {
              case CanvasElementType::Rect:
                rect_pipeline->Bind(PipelineBindPointGraphics);
                break;
              case CanvasElementType::Image:
              case CanvasElementType::Button:
              case CanvasElementType::UIDocument:
                image_pipeline->Bind(PipelineBindPointGraphics);
                break;
              case CanvasElementType::Text:
                text_pipeline->Bind(PipelineBindPointGraphics);
                break;
            }
            bound_type = entry.type;
            first = false;
          }

          uint32_t eid =
              (static_cast<uint32_t>(renderer->GetCurrentSceneIndex()) << 24) |
              (static_cast<uint32_t>(entry.entity) + 1);
          DrawCanvasElement(entry, renderer, element_layout,
                            textured_layout, eid);
        }
      });

  graph.PassWritesColor(canvas_pass, canvas_out);
  for (const auto& rml_tex : rml_offscreen_textures) {
    graph.PassReadsTexture(canvas_pass, rml_tex);
  }
  graph.SetPassFramebuffer(canvas_pass, pool->GetFramebuffer("canvas"));
  graph.SetPassViewport(canvas_pass, ctx.viewport_size);
  graph.SetPassClearColor(canvas_pass, {0, 0, 0, 0});

  registry.Register("CanvasOut", canvas_out);

  // ---- Composite pass: blend all canvas layers onto PipelineOutput ----
  RGResource canvas_comp_out =
      graph.ImportTexture("CanvasComp", pool->GetTexture("canvas_comp.color"));
  auto pipeline_input = registry.Get("PipelineOutput");
  auto comp_pipeline = comp_pipeline_;

  uint32_t canvas_comp = graph.AddPass(
      "CanvasComposite", comp_render_pass_,
      [comp_pipeline, pool, renderer, is_external](VkCommandBuffer) {
        comp_pipeline->Bind(PipelineBindPointGraphics);
        // Draw scene (previous PipelineOutput)
        renderer->DrawFullscreen(comp_pipeline,
                                 {pool->GetDescriptor("canvas_comp.input")});
        // Draw world canvas on top (alpha blended)
        renderer->DrawFullscreen(comp_pipeline,
                                 {pool->GetDescriptor("canvas_world.output")});
        if (!is_external) {
          // Per-canvas camera quads as fullscreen (game view only;
          // in editor they're drawn as 3D quads in the world pass)
          renderer->DrawFullscreen(
              comp_pipeline, {pool->GetDescriptor("canvas_camera.output")});
          // Overlay canvas (game view only; in editor it's in camera_list)
          renderer->DrawFullscreen(comp_pipeline,
                                   {pool->GetDescriptor("canvas.output")});
        }
      });

  graph.PassReadsTexture(canvas_comp, pipeline_input);
  graph.PassReadsTexture(canvas_comp, canvas_world_out);
  graph.PassReadsTexture(canvas_comp, canvas_camera_out);
  graph.PassReadsTexture(canvas_comp, canvas_out);
  graph.PassWritesColor(canvas_comp, canvas_comp_out);
  graph.SetPassFramebuffer(canvas_comp, pool->GetFramebuffer("canvas_comp"));
  graph.SetPassViewport(canvas_comp, ctx.viewport_size);
  graph.SetPassClearColor(canvas_comp, {0, 0, 0, 0});

  registry.Register("PipelineOutput", canvas_comp_out);
}

}  // namespace Wiesel
