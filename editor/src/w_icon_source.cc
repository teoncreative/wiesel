//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_icon_source.h"

#include <stb_image.h>

#include "util/w_vfs.h"
#include "w_engine.h"

namespace Wiesel::Editor {

void PngIconSource::AddIcon(uint32_t codepoint, const std::string& vfs_path) {
  auto file = Engine::vfs()->Open(vfs_path);
  if (!file) {
    LOG_WARN("Icon not found: {}", vfs_path);
    return;
  }

  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc* pixels =
      stbi_load_from_memory(file.Data(), static_cast<int>(file.Size()), &w, &h,
                            &channels, STBI_rgb_alpha);
  if (!pixels) {
    LOG_WARN("Failed to decode icon: {}", vfs_path);
    return;
  }

  SourceImage img;
  img.width = w;
  img.height = h;
  img.pixels.assign(pixels, pixels + w * h * 4);
  stbi_image_free(pixels);

  icons_[codepoint] = std::move(img);
}

bool PngIconSource::HasGlyph(uint32_t codepoint) const {
  return icons_.contains(codepoint);
}

IconBitmap PngIconSource::RasterizeGlyph(uint32_t codepoint, int size) const {
  auto it = icons_.find(codepoint);
  if (it == icons_.end() || size <= 0) {
    return {};
  }

  const SourceImage& src = it->second;
  auto* dst = static_cast<uint8_t*>(malloc(size * size * 4));
  if (!dst) {
    return {};
  }

  // Area-average (box filter) downscaling - averages all source pixels
  // that contribute to each destination pixel. Much better than bilinear
  // for large downscale ratios (e.g. 360px -> 14px).
  float scale_x = static_cast<float>(src.width) / size;
  float scale_y = static_cast<float>(src.height) / size;

  for (int y = 0; y < size; y++) {
    float src_y0 = y * scale_y;
    float src_y1 = (y + 1) * scale_y;

    for (int x = 0; x < size; x++) {
      float src_x0 = x * scale_x;
      float src_x1 = (x + 1) * scale_x;

      float accum[4] = {0, 0, 0, 0};
      float total_weight = 0;

      int iy0 = static_cast<int>(std::floor(src_y0));
      int iy1 = static_cast<int>(std::ceil(src_y1));
      int ix0 = static_cast<int>(std::floor(src_x0));
      int ix1 = static_cast<int>(std::ceil(src_x1));
      iy1 = std::min(iy1, src.height);
      ix1 = std::min(ix1, src.width);

      for (int iy = iy0; iy < iy1; iy++) {
        float wy = std::min(static_cast<float>(iy + 1), src_y1) -
                   std::max(static_cast<float>(iy), src_y0);
        for (int ix = ix0; ix < ix1; ix++) {
          float wx = std::min(static_cast<float>(ix + 1), src_x1) -
                     std::max(static_cast<float>(ix), src_x0);
          float w = wx * wy;
          int idx = (iy * src.width + ix) * 4;
          for (int c = 0; c < 4; c++) {
            accum[c] += src.pixels[idx + c] * w;
          }
          total_weight += w;
        }
      }

      int dst_idx = (y * size + x) * 4;
      for (int c = 0; c < 4; c++) {
        float val = (total_weight > 0) ? accum[c] / total_weight : 0;
        dst[dst_idx + c] = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
      }
    }
  }

  return {dst, size, size};
}

}  // namespace Wiesel::Editor
