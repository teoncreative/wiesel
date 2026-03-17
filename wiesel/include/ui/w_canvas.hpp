
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

struct CanvasComponent {
  CanvasType type = CanvasTypeScreenSpace;
  LayoutDirection direction = LayoutDirection::None;
  ChildAlignment alignment = ChildAlignment::Start;
  float spacing = 0.0f;
  int sort_order = 0;
};

struct CanvasRectComponent {
  glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

  // GPU resources (allocated lazily)
  std::shared_ptr<UniformBuffer> ubo_;
  std::shared_ptr<DescriptorSet> descriptor_;
  bool gpu_dirty_ = true;
};

struct CanvasImageComponent {
  std::shared_ptr<Texture> texture;
  glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};

  // GPU resources (allocated lazily)
  std::shared_ptr<UniformBuffer> ubo_;
  std::shared_ptr<DescriptorSet> descriptor_;
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

}  // namespace Wiesel
