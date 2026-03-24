//
//   Copyright 2025 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <glm/glm.hpp>
#include "asset/w_asset_handle.hpp"

namespace Wiesel {

// Runtime data for a .wsprite asset - a sub-region of a texture.
struct SpriteAssetData {
  AssetHandle texture_handle;
  glm::vec4 rect = {0, 0, 0, 0};  // x, y, width, height in pixels
  glm::vec2 pivot = {0.5f, 0.5f};

  // Compute UV rect from pixel rect and texture dimensions.
  glm::vec4 GetUVRect(float tex_width, float tex_height) const {
    if (tex_width <= 0 || tex_height <= 0) {
      return {0, 0, 1, 1};
    }
    return {rect.x / tex_width, rect.y / tex_height, rect.z / tex_width,
            rect.w / tex_height};
  }
};

}  // namespace Wiesel