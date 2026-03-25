
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_font.h"
#include "asset/w_asset_manager.h"
#include "util/w_vfs.h"
#include "w_engine.h"

namespace Wiesel {

// --- FontAsset ---

FontAsset::FontAsset(const std::string& vfs_path) {
  FT_Error error = FT_Init_FreeType(&ft_library_);
  if (error) {
    LOG_ERROR("FreeType init failed");
    return;
  }

  std::shared_ptr<VirtualFileSystem> vfs = Engine::vfs();
  if (!vfs->FileExists(vfs_path)) {
    LOG_ERROR("Font file not found: {}", vfs_path);
    FT_Done_FreeType(ft_library_);
    ft_library_ = nullptr;
    return;
  }
  VfsFile file = vfs->Open(vfs_path);
  font_data_.assign(file.Data(), file.Data() + file.Size());

  error =
      FT_New_Memory_Face(ft_library_, font_data_.data(),
                         static_cast<FT_Long>(font_data_.size()), 0, &ft_face_);
  if (error) {
    LOG_ERROR("Failed to load font face: {}", vfs_path);
    FT_Done_FreeType(ft_library_);
    ft_library_ = nullptr;
    return;
  }

  LOG_INFO("FontAsset loaded: {}", vfs_path);
}

FontAsset::~FontAsset() {
  if (ft_face_) {
    FT_Done_Face(ft_face_);
  }
  if (ft_library_) {
    FT_Done_FreeType(ft_library_);
  }
}

// --- Font (size-specific rasterized instance) ---

Font::Font(std::shared_ptr<FontAsset> asset, float size_px)
    : asset_(std::move(asset)), native_size_(size_px) {
  if (!asset_ || !asset_->IsLoaded()) {
    return;
  }

  FT_Face face = asset_->GetFace();
  FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(size_px));
  line_height_ = static_cast<float>(face->size->metrics.height >> 6);
  ascent_ = static_cast<float>(face->size->metrics.ascender >> 6);
  descent_ = static_cast<float>(-(face->size->metrics.descender >> 6));

  // Pre-rasterize common Unicode ranges
  atlas_pixels_.resize(atlas_width_ * atlas_height_, 0);
  for (uint32_t c = 0x0020; c <= 0x007E; c++) {
    RasterizeGlyph(c);
  }
  for (uint32_t c = 0x00A0; c <= 0x00FF; c++) {
    RasterizeGlyph(c);
  }
  for (uint32_t c = 0x0100; c <= 0x017F; c++) {
    RasterizeGlyph(c);
  }
  for (uint32_t c = 0x0180; c <= 0x024F; c++) {
    RasterizeGlyph(c);
  }
  for (uint32_t c = 0x2000; c <= 0x206F; c++) {
    RasterizeGlyph(c);
  }

  UploadAtlas();
  loaded_ = true;
}

Font::~Font() = default;

const GlyphInfo* Font::GetGlyph(uint32_t codepoint) {
  auto it = glyphs_.find(codepoint);
  if (it != glyphs_.end()) {
    return &it->second;
  }

  if (!asset_ || !asset_->IsLoaded()) {
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
    if (!g) {
      continue;
    }

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

  float width;
  if (last_visible) {
    width =
        (last_cursor + last_visible->bearing.x + last_visible->size.x) * scale;
  } else {
    width = cursor * scale;
  }

  float height = (ascent_ + descent_) * scale;
  return {width, height};
}

void Font::RasterizeGlyph(uint32_t codepoint) {
  if (!asset_ || !asset_->IsLoaded()) {
    return;
  }

  FT_Face face = asset_->GetFace();
  // Need to set size each time since FontAsset's face is shared across sizes
  FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(native_size_));

  FT_Error error = FT_Load_Char(face, codepoint, FT_LOAD_RENDER);
  if (error) {
    return;
  }
  FT_GlyphSlot g = face->glyph;

  if (cursor_x_ + g->bitmap.width + 2 > atlas_width_) {
    cursor_x_ = 0;
    cursor_y_ += row_height_ + 2;
    row_height_ = 0;
  }

  if (cursor_y_ + g->bitmap.rows > atlas_height_) {
    LOG_WARN("Font atlas overflow for codepoint {}", codepoint);
    return;
  }

  FontAAMode aa_mode = asset_->GetAAMode();
  for (uint32_t y = 0; y < g->bitmap.rows; y++) {
    for (uint32_t x = 0; x < g->bitmap.width; x++) {
      uint8_t value = g->bitmap.buffer[y * g->bitmap.pitch + x];
      if (aa_mode == FontAAMode::None) {
        value = (value > 127) ? 255 : 0;
      }
      atlas_pixels_[(cursor_y_ + y) * atlas_width_ + (cursor_x_ + x)] = value;
    }
  }

  GlyphInfo info;
  info.size = {g->bitmap.width, g->bitmap.rows};
  info.bearing = {g->bitmap_left, g->bitmap_top};
  info.advance = static_cast<int32_t>(g->advance.x);
  info.uv_min = {static_cast<float>(cursor_x_) / atlas_width_,
                 static_cast<float>(cursor_y_) / atlas_height_};
  info.uv_max = {
      static_cast<float>(cursor_x_ + g->bitmap.width) / atlas_width_,
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

  atlas_texture_ =
      renderer->CreateTexture(atlas_pixels_.data(), 1, props, sampler);
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
    i++;
    return 0xFFFD;
  }

  i++;
  for (int j = 0; j < extra && i < str.size(); j++, i++) {
    uint8_t b = static_cast<uint8_t>(str[i]);
    if ((b & 0xC0) != 0x80) {
      return 0xFFFD;
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

// --- FontCache ---

std::unordered_map<std::string, std::shared_ptr<Font>> FontCache::cache_;

std::shared_ptr<Font> FontCache::Get(AssetHandle font_handle, float size) {
  float raster_size = std::ceil(size);
  std::string handle_str =
      font_handle.IsValid() ? font_handle.ToString() : "default";
  std::string key =
      handle_str + "_" + std::to_string(static_cast<int>(raster_size));

  auto it = cache_.find(key);
  if (it != cache_.end()) {
    return it->second;
  }

  // Get or create FontAsset from AssetManager
  std::shared_ptr<FontAsset> asset;
  if (font_handle.IsValid()) {
    asset = Engine::asset_manager().Get<FontAsset>(font_handle);
    if (!asset) {
      // Load on demand
      const auto* meta = Engine::asset_manager().GetMetadata(font_handle);
      if (meta) {
        asset = std::make_shared<FontAsset>(meta->virtual_source_path);
        // Apply AA mode from asset properties
        const auto* props = meta->GetProperties<FontAssetProperties>();
        if (props) {
          asset->SetAAMode(props->aa_mode);
        }
        Engine::asset_manager().Store<FontAsset>(font_handle, asset);
        Engine::asset_manager().SetLoadState(
            font_handle, AssetLoadState::Unloaded, AssetLoadState::Loaded);
      }
    }
  }

  if (!asset) {
    // Fallback to default engine font
    static std::shared_ptr<FontAsset> default_asset;
    if (!default_asset) {
      default_asset = std::make_shared<FontAsset>("engine://fonts/default.ttf");
    }
    asset = default_asset;
  }

  auto font = std::make_shared<Font>(asset, raster_size);
  cache_[key] = font;
  return font;
}

void FontCache::Invalidate(AssetHandle font_handle) {
  std::string prefix =
      font_handle.IsValid() ? font_handle.ToString() : "default";
  for (auto it = cache_.begin(); it != cache_.end();) {
    if (it->first.find(prefix) == 0) {
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
}

void FontCache::Clear() {
  cache_.clear();
}

}  // namespace Wiesel