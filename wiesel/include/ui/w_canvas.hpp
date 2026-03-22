
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "asset/w_asset_handle.hpp"
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

enum class SizeMode {
  Fixed,
  Percent
};

enum class LayoutDirection {
  None,
  Row,
  Column
};

enum class ChildAlignment {
  Start,
  Center,
  End
};

enum CanvasType { CanvasTypeScreenSpace };

enum class ScaleMode {
  ConstantPixelSize,   // 1:1 pixel mapping, no scaling
  ScaleWithScreenSize  // scale relative to a reference resolution
};

struct CanvasComponent {
  CanvasType type = CanvasTypeScreenSpace;
  LayoutDirection direction = LayoutDirection::None;
  ChildAlignment alignment = ChildAlignment::Start;
  float spacing = 0.0f;
  float start_spacing = 0.0f;  // space before first child
  float end_spacing = 0.0f;    // space after last child
  int sort_order = 0;
};

struct CanvasScalerComponent {
  ScaleMode scale_mode = ScaleMode::ConstantPixelSize;
  glm::vec2 reference_resolution = {1920.0f, 1080.0f};

  // 0 = match width, 1 = match height, 0.5 = blend both
  float match_width_or_height = 0.5f;
};

struct CanvasRectComponent {
  glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

  // GPU resources (allocated lazily)
  std::shared_ptr<UniformBuffer> ubo_;
  std::shared_ptr<DescriptorSet> descriptor_;
  bool gpu_dirty_ = true;
};

enum class ButtonState : int {
  Normal = 0,
  Hovered = 1,
  Pressed = 2,
  Disabled = 3,
};

// Button component — provides automatic visual state management.
// Works with a sibling CanvasImageComponent or CanvasRectComponent.
// Requires InteractableComponent for hit detection.
// Click handling is done via OnPointerClick in scripts.
struct ButtonComponent {
  // Colors for each state (applied to sibling CanvasImage tint or CanvasRect color)
  glm::vec4 normal_color = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 hovered_color = {0.9f, 0.9f, 0.9f, 1.0f};
  glm::vec4 pressed_color = {0.7f, 0.7f, 0.7f, 1.0f};
  glm::vec4 disabled_color = {0.5f, 0.5f, 0.5f, 0.5f};

  // Optional texture per state (if set, swaps CanvasImage texture on state change)
  AssetHandle normal_texture;
  AssetHandle hovered_texture;
  AssetHandle pressed_texture;
  AssetHandle disabled_texture;

  // Runtime state (not serialized)
  ButtonState state_ = ButtonState::Normal;
};

struct CanvasImageComponent {
  std::shared_ptr<Texture> texture;
  glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};

  // 9-slice: border sizes in pixels (left, top, right, bottom)
  // When all zeros, renders as a normal stretched image.
  glm::vec4 slice_border = {0.0f, 0.0f, 0.0f, 0.0f};

  bool IsSliced() const {
    return slice_border.x > 0 || slice_border.y > 0 ||
           slice_border.z > 0 || slice_border.w > 0;
  }

  // GPU resources (allocated lazily)
  std::shared_ptr<UniformBuffer> ubo_;
  std::shared_ptr<DescriptorSet> descriptor_;
  // 9-slice needs 9 quads — center reuses ubo_/descriptor_, 8 borders get their own
  std::vector<std::shared_ptr<UniformBuffer>> slice_ubos_;
  std::vector<std::shared_ptr<DescriptorSet>> slice_descriptors_;
  bool gpu_dirty_ = true;
};

struct TextGlyphGPU {
  std::shared_ptr<UniformBuffer> ubo;
  std::shared_ptr<DescriptorSet> descriptor;
};

struct TextComponent {
  std::string text;
  std::string font_path = "/engine/fonts/default.ttf";
  float font_size = 16.0f;
  glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

  // Per-glyph GPU resources (one UBO+descriptor per visible character)
  std::vector<TextGlyphGPU> glyph_gpu_;
  std::string prev_text_;
  std::string prev_font_path_;
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
