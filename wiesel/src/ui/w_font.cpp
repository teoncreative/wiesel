
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_font.hpp"
#include "util/w_vfs.hpp"
#include "w_engine.hpp"

namespace Wiesel {

Font::Font(const std::string& vfs_path, float size_px) : native_size_(size_px) {
  FT_Error error = FT_Init_FreeType(&ft_library_);
  if (error) {
    LOG_ERROR("FreeType init failed");
    return;
  }

  // Load font file via VFS - keep data alive for FreeType
  std::shared_ptr<VirtualFileSystem> vfs = Engine::vfs();
  if (!vfs->FileExists(vfs_path)) {
    LOG_ERROR("Font file not found: {}", vfs_path);
    FT_Done_FreeType(ft_library_);
    ft_library_ = nullptr;
    return;
  }
  VfsFile file = vfs->Open(vfs_path);
  font_data_.assign(file.Data(), file.Data() + file.Size());

  error = FT_New_Memory_Face(ft_library_, font_data_.data(),
                             static_cast<FT_Long>(font_data_.size()), 0,
                             &ft_face_);
  if (error) {
    LOG_ERROR("Failed to load font face: {}", vfs_path);
    FT_Done_FreeType(ft_library_);
    ft_library_ = nullptr;
    return;
  }

  FT_Set_Pixel_Sizes(ft_face_, 0, static_cast<FT_UInt>(size_px));
  line_height_ = static_cast<float>(ft_face_->size->metrics.height >> 6);
  ascent_ = static_cast<float>(ft_face_->size->metrics.ascender >> 6);
  descent_ = static_cast<float>(-(ft_face_->size->metrics.descender >> 6));

  // Pre-rasterize common Unicode ranges so on-demand rasterization is rare
  atlas_pixels_.resize(atlas_width_ * atlas_height_, 0);
  // ASCII printable (U+0020 - U+007E)
  for (uint32_t c = 0x0020; c <= 0x007E; c++) RasterizeGlyph(c);
  // Latin-1 Supplement (U+00A0 - U+00FF): accented Western European
  for (uint32_t c = 0x00A0; c <= 0x00FF; c++) RasterizeGlyph(c);
  // Latin Extended-A (U+0100 - U+017F): Turkish ğ/ş/ı, Polish, Czech, etc.
  for (uint32_t c = 0x0100; c <= 0x017F; c++) RasterizeGlyph(c);
  // Latin Extended-B (U+0180 - U+024F)
  for (uint32_t c = 0x0180; c <= 0x024F; c++) RasterizeGlyph(c);
  // General Punctuation (U+2000 - U+206F): em dash, bullets, ellipsis
  for (uint32_t c = 0x2000; c <= 0x206F; c++) RasterizeGlyph(c);

  UploadAtlas();
  loaded_ = true;
}

Font::~Font() {
  if (ft_face_) {
    FT_Done_Face(ft_face_);
  }
  if (ft_library_) {
    FT_Done_FreeType(ft_library_);
  }
}

const GlyphInfo* Font::GetGlyph(uint32_t codepoint) {
  auto it = glyphs_.find(codepoint);
  if (it != glyphs_.end()) {
    return &it->second;
  }

  // Rasterize on demand for non-ASCII codepoints
  if (!ft_face_) {
    return nullptr;
  }
  RasterizeGlyph(codepoint);
  atlas_dirty_ = true;

  it = glyphs_.find(codepoint);
  if (it == glyphs_.end()) {
    return nullptr;
  }
  return &it->second;
}

glm::vec2 Font::MeasureText(const std::string& text, float font_size) {
  float scale = font_size / native_size_;
  float cursor = 0;
  float max_ascender = 0;
  float max_descender = 0;
  const GlyphInfo* last_visible = nullptr;
  float last_cursor = 0;

  for (size_t i = 0; i < text.size();) {
    uint32_t cp = DecodeUTF8(text, i);
    const GlyphInfo* g = GetGlyph(cp);
    if (!g) continue;

    // Track vertical extents from actual glyphs
    if (g->size.y > 0) {
      float asc = static_cast<float>(g->bearing.y);
      float desc = static_cast<float>(g->size.y - g->bearing.y);
      max_ascender = std::max(max_ascender, asc);
      max_descender = std::max(max_descender, desc);
      last_visible = g;
      last_cursor = cursor;
    }
    cursor += (g->advance >> 6);
  }

  // Width: use tight right edge of last visible glyph instead of trailing advance
  float width;
  if (last_visible) {
    width = (last_cursor + last_visible->bearing.x + last_visible->size.x) * scale;
  } else {
    width = cursor * scale;
  }

  // Use face-level ascent + descent for a fixed, predictable height.
  // This ensures all strings at the same font size produce the same box height
  // regardless of which characters are present (e.g. "A" vs "Şg").
  // Matches DrawCanvasText which positions the baseline at ascent_ from the top.
  float height = (ascent_ + descent_) * scale;
  return {width, height};
}

void Font::RasterizeGlyph(uint32_t codepoint) {
  FT_Error error = FT_Load_Char(ft_face_, codepoint, FT_LOAD_RENDER);
  if (error) {
    return;
  }
  FT_GlyphSlot g = ft_face_->glyph;

  // Row packing: if glyph doesn't fit on current row, advance to next
  if (cursor_x_ + g->bitmap.width + 2 > atlas_width_) {
    cursor_x_ = 0;
    cursor_y_ += row_height_ + 2;
    row_height_ = 0;
  }

  // Check if we've run out of atlas space
  if (cursor_y_ + g->bitmap.rows > atlas_height_) {
    LOG_WARN("Font atlas overflow for codepoint {}", codepoint);
    return;
  }

  // Copy bitmap into atlas
  for (uint32_t y = 0; y < g->bitmap.rows; y++) {
    for (uint32_t x = 0; x < g->bitmap.width; x++) {
      atlas_pixels_[(cursor_y_ + y) * atlas_width_ + (cursor_x_ + x)] =
          g->bitmap.buffer[y * g->bitmap.pitch + x];
    }
  }

  GlyphInfo info;
  info.size = {g->bitmap.width, g->bitmap.rows};
  info.bearing = {g->bitmap_left, g->bitmap_top};
  info.advance = static_cast<int32_t>(g->advance.x);
  info.uv_min = {static_cast<float>(cursor_x_) / atlas_width_,
                 static_cast<float>(cursor_y_) / atlas_height_};
  info.uv_max = {static_cast<float>(cursor_x_ + g->bitmap.width) / atlas_width_,
                 static_cast<float>(cursor_y_ + g->bitmap.rows) / atlas_height_};
  glyphs_[codepoint] = info;

  row_height_ = std::max(row_height_, g->bitmap.rows);
  cursor_x_ += g->bitmap.width + 2;
}

void Font::UploadAtlas() {
  PROFILE_ZONE_SCOPED_N("Font::UploadAtlas");
  std::shared_ptr<Renderer> renderer = Engine::renderer();
  TextureProps props{};
  props.type = TextureTypeNone;
  props.generate_mipmaps = false;
  props.image_format = VK_FORMAT_R8_UNORM;
  props.width = atlas_width_;
  props.height = atlas_height_;

  SamplerProps sampler{};
  sampler.MagFilter = VK_FILTER_LINEAR;
  sampler.MinFilter = VK_FILTER_LINEAR;
  sampler.AddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  atlas_texture_ = renderer->CreateTexture(atlas_pixels_.data(), 1, props, sampler);
  atlas_image_view_ = atlas_texture_->image_view_;
}

uint32_t Font::DecodeUTF8(const std::string& str, size_t& i) {
  uint8_t c = static_cast<uint8_t>(str[i]);
  uint32_t cp;
  int extra;

  if (c < 0x80) {
    cp = c;
    extra = 0;
  } else if ((c & 0xE0) == 0xC0) {
    cp = c & 0x1F;
    extra = 1;
  } else if ((c & 0xF0) == 0xE0) {
    cp = c & 0x0F;
    extra = 2;
  } else if ((c & 0xF8) == 0xF0) {
    cp = c & 0x07;
    extra = 3;
  } else {
    // Invalid lead byte, skip it
    i++;
    return 0xFFFD;  // replacement character
  }

  i++;
  for (int j = 0; j < extra && i < str.size(); j++, i++) {
    uint8_t b = static_cast<uint8_t>(str[i]);
    if ((b & 0xC0) != 0x80) {
      return 0xFFFD;  // invalid continuation
    }
    cp = (cp << 6) | (b & 0x3F);
  }
  return cp;
}

bool Font::FlushAtlas() {
  if (!atlas_dirty_) {
    return false;
  }
  UploadAtlas();
  atlas_dirty_ = false;
  return true;
}

// FontCache implementation
std::unordered_map<std::string, std::shared_ptr<Font>> FontCache::cache_;

std::shared_ptr<Font> FontCache::Get(const std::string& path, float size) {
  // Rasterize at the nearest integer pixel size for crisp 1:1 rendering.
  float raster_size = std::ceil(size);
  std::string key = path + "_" + std::to_string(static_cast<int>(raster_size));
  auto it = cache_.find(key);
  if (it != cache_.end()) {
    return it->second;
  }
  std::shared_ptr<Font> font = std::make_shared<Font>(path, raster_size);
  cache_[key] = font;
  return font;
}

}  // namespace Wiesel
