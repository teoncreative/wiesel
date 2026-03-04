
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

#include <ft2build.h>
#include FT_FREETYPE_H

#include "rendering/w_texture.hpp"
#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {

struct GlyphInfo {
  glm::vec2 uv_min = {0, 0};
  glm::vec2 uv_max = {0, 0};
  glm::ivec2 size = {0, 0};
  glm::ivec2 bearing = {0, 0};
  int32_t advance = 0;
};

class Font {
 public:
  Font(const std::string& vfs_path, float size_px = 32.0f);
  ~Font();

  bool IsLoaded() const { return loaded_; }
  const GlyphInfo* GetGlyph(uint32_t codepoint);
  Ref<ImageView> GetAtlasImageView() const { return atlas_image_view_; }
  float GetLineHeight() const { return line_height_; }
  float GetAscent() const { return ascent_; }
  float GetNativeSize() const { return native_size_; }
  glm::vec2 MeasureText(const std::string& text, float font_size);

  // Decode one UTF-8 codepoint from a string, advancing the index.
  static uint32_t DecodeUTF8(const std::string& str, size_t& i);

  // Re-upload atlas GPU texture if new glyphs were rasterized on demand.
  // Returns true if the atlas was re-uploaded (descriptors need rebuilding).
  bool FlushAtlas();

 private:
  void RasterizeGlyph(uint32_t codepoint);
  void UploadAtlas();

  FT_Library ft_library_ = nullptr;
  FT_Face ft_face_ = nullptr;
  float native_size_ = 0;
  float line_height_ = 0;
  float ascent_ = 0;

  std::unordered_map<uint32_t, GlyphInfo> glyphs_;

  uint32_t atlas_width_ = 2048;
  uint32_t atlas_height_ = 2048;
  uint32_t cursor_x_ = 0;
  uint32_t cursor_y_ = 0;
  uint32_t row_height_ = 0;
  std::vector<uint8_t> atlas_pixels_;

  // Persistent font file data (FreeType needs it to stay alive)
  std::vector<uint8_t> font_data_;

  Ref<Texture> atlas_texture_;
  Ref<ImageView> atlas_image_view_;
  bool loaded_ = false;
  bool atlas_dirty_ = false;
};

class FontCache {
 public:
  static Ref<Font> Get(const std::string& path, float size);

 private:
  static std::unordered_map<std::string, Ref<Font>> cache_;
};

}  // namespace Wiesel
