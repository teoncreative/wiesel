
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_label_texture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "util/w_logger.hpp"
#include "util/w_vfs.hpp"
#include "w_engine.hpp"

namespace Wiesel {

static std::unordered_map<std::string, std::shared_ptr<Texture>> s_label_cache;

std::shared_ptr<Texture> GetOrCreateLabelTexture(const std::string& key,
                                                 const std::string& text,
                                                 const glm::vec4& bg_color,
                                                 const glm::vec4& text_color,
                                                 int font_size) {

  auto it = s_label_cache.find(key);
  if (it != s_label_cache.end()) {
    return it->second;
  }

  // Init FreeType and load the default font
  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    LOG_ERROR("FreeType init failed for label texture");
    return nullptr;
  }

  auto vfs = Engine::vfs();
  std::string font_path = "/engine/fonts/default.ttf";
  if (!vfs->FileExists(font_path)) {
    LOG_ERROR("Default font not found for label texture");
    FT_Done_FreeType(ft);
    return nullptr;
  }

  VfsFile font_file = vfs->Open(font_path);
  std::vector<uint8_t> font_data(font_file.Data(),
                                 font_file.Data() + font_file.Size());

  FT_Face face;
  if (FT_New_Memory_Face(ft, font_data.data(),
                         static_cast<FT_Long>(font_data.size()), 0, &face)) {
    LOG_ERROR("Failed to load font face for label texture");
    FT_Done_FreeType(ft);
    return nullptr;
  }

  FT_Set_Pixel_Sizes(face, 0, font_size);

  // Measure text width
  int total_width = 0;
  int max_ascent = 0;
  int max_descent = 0;
  for (char c : text) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      continue;
    }
    total_width += static_cast<int>(face->glyph->advance.x >> 6);
    int ascent = face->glyph->bitmap_top;
    int descent = static_cast<int>(face->glyph->bitmap.rows) - ascent;
    if (ascent > max_ascent) {
      max_ascent = ascent;
    }
    if (descent > max_descent) {
      max_descent = descent;
    }
  }

  int padding = 12;
  int tex_w = total_width + padding * 2;
  int tex_h = max_ascent + max_descent + padding * 2;

  // Power-of-two not required for Vulkan, but round up to even numbers
  if (tex_w % 2 != 0) {
    tex_w++;
  }
  if (tex_h % 2 != 0) {
    tex_h++;
  }

  // Create RGBA pixel buffer with background color
  std::vector<uint8_t> pixels(tex_w * tex_h * 4);
  uint8_t bg_r = static_cast<uint8_t>(bg_color.r * 255.0f);
  uint8_t bg_g = static_cast<uint8_t>(bg_color.g * 255.0f);
  uint8_t bg_b = static_cast<uint8_t>(bg_color.b * 255.0f);
  uint8_t bg_a = static_cast<uint8_t>(bg_color.a * 255.0f);
  for (int i = 0; i < tex_w * tex_h; i++) {
    pixels[i * 4 + 0] = bg_r;
    pixels[i * 4 + 1] = bg_g;
    pixels[i * 4 + 2] = bg_b;
    pixels[i * 4 + 3] = bg_a;
  }

  // Render glyphs
  uint8_t txt_r = static_cast<uint8_t>(text_color.r * 255.0f);
  uint8_t txt_g = static_cast<uint8_t>(text_color.g * 255.0f);
  uint8_t txt_b = static_cast<uint8_t>(text_color.b * 255.0f);
  uint8_t txt_a = static_cast<uint8_t>(text_color.a * 255.0f);

  int pen_x = padding;
  int baseline_y = padding + max_ascent;

  for (char c : text) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      continue;
    }

    FT_Bitmap& bmp = face->glyph->bitmap;
    int glyph_x = pen_x + face->glyph->bitmap_left;
    int glyph_y = baseline_y - face->glyph->bitmap_top;

    for (unsigned int row = 0; row < bmp.rows; row++) {
      for (unsigned int col = 0; col < bmp.width; col++) {
        int px = glyph_x + static_cast<int>(col);
        int py = glyph_y + static_cast<int>(row);
        if (px < 0 || px >= tex_w || py < 0 || py >= tex_h) {
          continue;
        }

        uint8_t alpha = bmp.buffer[row * bmp.pitch + col];
        if (alpha == 0) {
          continue;
        }

        int idx = (py * tex_w + px) * 4;
        // Alpha blend text over background
        float a = static_cast<float>(alpha) / 255.0f;
        pixels[idx + 0] =
            static_cast<uint8_t>(txt_r * a + pixels[idx + 0] * (1.0f - a));
        pixels[idx + 1] =
            static_cast<uint8_t>(txt_g * a + pixels[idx + 1] * (1.0f - a));
        pixels[idx + 2] =
            static_cast<uint8_t>(txt_b * a + pixels[idx + 2] * (1.0f - a));
        pixels[idx + 3] =
            static_cast<uint8_t>(txt_a * a + pixels[idx + 3] * (1.0f - a));
      }
    }

    pen_x += static_cast<int>(face->glyph->advance.x >> 6);
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  // Upload to GPU
  TextureProps props;
  props.type = TextureTypeDiffuse;
  props.generate_mipmaps = false;
  props.image_format = VK_FORMAT_R8G8B8A8_UNORM;
  props.width = tex_w;
  props.height = tex_h;

  auto texture = Engine::renderer()->CreateTexture(pixels.data(), 4, props, {});

  if (texture) {
    s_label_cache[key] = texture;
    LOG_INFO("Generated label texture '{}' ({}x{})", text, tex_w, tex_h);
  }

  return texture;
}

}  // namespace Wiesel