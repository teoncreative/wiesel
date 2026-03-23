
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

#include "asset/w_asset_handle.hpp"
#include "asset/w_asset_properties.hpp"
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

// FontAsset: loaded font file stored in AssetManager.
// Holds the raw data and FreeType face. One per font file.
class FontAsset {
 public:
  FontAsset(const std::string& vfs_path);
  ~FontAsset();

  bool IsLoaded() const { return ft_face_ != nullptr; }

  FT_Face GetFace() const { return ft_face_; }

  FontAAMode GetAAMode() const { return aa_mode_; }

  void SetAAMode(FontAAMode mode) { aa_mode_ = mode; }

 private:
  FT_Library ft_library_ = nullptr;
  FT_Face ft_face_ = nullptr;
  FontAAMode aa_mode_ = FontAAMode::Grayscale;
  std::vector<uint8_t> font_data_;
};

// Font: size-specific rasterized instance created from a FontAsset.
// Holds the glyph atlas for a specific pixel size.
class Font {
 public:
  Font(std::shared_ptr<FontAsset> asset, float size_px);
  ~Font();

  bool IsLoaded() const { return loaded_; }

  const GlyphInfo* GetGlyph(uint32_t codepoint);

  std::shared_ptr<ImageView> GetAtlasImageView() const {
    return atlas_image_view_;
  }

  float GetLineHeight() const { return line_height_; }

  float GetAscent() const { return ascent_; }

  float GetDescent() const { return descent_; }

  float GetNativeSize() const { return native_size_; }

  glm::vec2 MeasureText(const std::string& text, float font_size);

  static uint32_t DecodeUTF8(const std::string& str, size_t& i);

  // Re-upload atlas if new glyphs were rasterized on demand.
  bool FlushAtlas();

 private:
  void RasterizeGlyph(uint32_t codepoint);
  void UploadAtlas();

  std::shared_ptr<FontAsset> asset_;
  float native_size_ = 0;
  float line_height_ = 0;
  float ascent_ = 0;
  float descent_ = 0;

  std::unordered_map<uint32_t, GlyphInfo> glyphs_;

  uint32_t atlas_width_ = 2048;
  uint32_t atlas_height_ = 2048;
  uint32_t cursor_x_ = 0;
  uint32_t cursor_y_ = 0;
  uint32_t row_height_ = 0;
  std::vector<uint8_t> atlas_pixels_;

  std::shared_ptr<Texture> atlas_texture_;
  std::shared_ptr<ImageView> atlas_image_view_;
  bool loaded_ = false;
  bool atlas_dirty_ = false;
};

// FontCache: manages size-specific Font instances.
// Gets FontAsset from AssetManager, creates Font at requested size.
class FontCache {
 public:
  static std::shared_ptr<Font> Get(AssetHandle font_handle, float size);
  static void Invalidate(AssetHandle font_handle);
  static void Clear();

 private:
  // Key: "handle_string_size" -> Font
  static std::unordered_map<std::string, std::shared_ptr<Font>> cache_;
};

}  // namespace Wiesel