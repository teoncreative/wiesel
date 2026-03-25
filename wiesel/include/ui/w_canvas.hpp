
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "asset/w_asset_handle.hpp"
#include "entt/entity/entity.hpp"
#include "events/w_events.hpp"
#include "rendering/w_buffer.hpp"
#include "rendering/w_descriptor.hpp"
#include "rendering/w_texture.hpp"
#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {

enum class AnchorPreset {
  TopLeft,
  TopCenter,
  TopRight,
  MiddleLeft,
  MiddleCenter,
  MiddleRight,
  BottomLeft,
  BottomCenter,
  BottomRight,
  StretchAll
};

enum class SizeMode { Fixed, Percent };

enum class LayoutDirection { None, Row, Column };

enum class ChildAlignment { Start, Center, End };

enum class CanvasRenderMode {
  ScreenSpaceOverlay,  // 2D overlay on top of everything
  ScreenSpaceCamera,   // flat plane at distance in front of camera
  WorldSpace,          // 3D positioned via TransformComponent
};

enum class ScaleMode {
  ConstantPixelSize,   // 1:1 pixel mapping, no scaling
  ScaleWithScreenSize  // scale relative to a reference resolution
};

struct CanvasComponent {
  CanvasRenderMode render_mode = CanvasRenderMode::ScreenSpaceOverlay;
  LayoutDirection direction = LayoutDirection::None;
  ChildAlignment alignment = ChildAlignment::Start;
  float spacing = 0.0f;
  float start_spacing = 0.0f;  // space before first child
  float end_spacing = 0.0f;    // space after last child
  int sort_order = 0;

  // ScreenSpaceCamera settings
  float plane_distance = 10.0f;
  entt::entity camera_entity =
      entt::null;  // which camera to render in front of

  // Which player's input drives this canvas.
  // Overlay canvases default to player 0.
  int player_index = 0;
};

struct CanvasScalerComponent {
  ScaleMode scale_mode = ScaleMode::ConstantPixelSize;
  glm::vec2 reference_resolution = {1920.0f, 1080.0f};

  // 0 = match width, 1 = match height, 0.5 = blend both (ScreenSpace modes)
  float match_width_or_height = 0.5f;

  // WorldSpace: how many canvas pixels correspond to 1 world unit
  float reference_pixels_per_unit = 100.0f;
};

struct CanvasRectComponent {
  // Serialized
  glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

  // Runtime (not serialized) - GPU resources allocated lazily
  std::shared_ptr<UniformBuffer> ubo_;
  std::shared_ptr<DescriptorSet> descriptor_;
  bool gpu_dirty_ = true;
};

enum class ButtonState : int {
  Normal = 0,
  Hovered = 1,
  Pressed = 2,
  Selected = 3,
  Disabled = 4,
};

// Button component - self-rendering canvas element with per-state textures.
// Requires InteractableComponent for hit detection.
// Click handling is done via OnPointerClick in scripts.
struct ButtonComponent {
  // Tint per state
  glm::vec4 normal_color = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 hovered_color = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 pressed_color = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 selected_color = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 disabled_color = {0.5f, 0.5f, 0.5f, 0.5f};

  // Texture per state (normal is required, others fall back to normal)
  AssetHandle normal_texture;
  AssetHandle hovered_texture;
  AssetHandle pressed_texture;
  AssetHandle selected_texture;
  AssetHandle disabled_texture;

  // Child offset applied per state (pixels, affects children positioning)
  glm::vec2 hovered_offset = {0.0f, 0.0f};
  glm::vec2 pressed_offset = {0.0f, 0.0f};
  glm::vec2 selected_offset = {0.0f, 0.0f};

  // Runtime state (not serialized)
  ButtonState state_ = ButtonState::Normal;
};

struct CanvasImageComponent {
  AssetHandle texture_handle;
  glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
};

struct TextGlyphGPU {
  std::shared_ptr<UniformBuffer> ubo;
  std::shared_ptr<DescriptorSet> descriptor;
};

struct TextComponent {
  // Serialized
  std::string text;
  AssetHandle font_handle;  // font asset (empty = default engine font)
  float font_size = 16.0f;
  glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
  bool shadow = false;
  glm::vec2 shadow_offset = {1.0f, 1.0f};
  glm::vec4 shadow_color = {0.0f, 0.0f, 0.0f, 0.5f};

  // Runtime (not serialized) - per-glyph GPU resources and change tracking
  std::vector<TextGlyphGPU> glyph_gpu_;
  std::string prev_text_;
  AssetHandle prev_font_handle_;
  float prev_font_size_ = 0.0f;
  bool gpu_dirty_ = true;
};

struct TextInputComponent {
  std::string text;
  std::string placeholder = "Enter text...";
  int max_length = 0;  // 0 = unlimited
  glm::vec4 cursor_color = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 placeholder_color = {0.5f, 0.5f, 0.5f, 1.0f};

  // Runtime state (not serialized)
  int cursor_pos_ = 0;
  bool focused_ = false;
  float cursor_timer_ = 0.0f;
  bool cursor_visible_ = true;
};

}  // namespace Wiesel
