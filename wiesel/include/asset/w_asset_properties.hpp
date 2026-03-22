
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

#include <glm/glm.hpp>
#include <cstdint>

namespace Wiesel {

// --- Texture asset properties ---

enum class TextureFilterMode : uint8_t {
  Nearest = 0,
  Linear = 1
};

enum class TextureWrapMode : uint8_t {
  Repeat = 0,
  Clamp = 1,
  Mirror = 2
};

enum class TextureAssetType : uint8_t {
  Default = 0,   // sRGB, mipmaps - general purpose 3D textures
  NormalMap = 1,  // linear (UNORM), mipmaps - normal/height/roughness/metallic
  Sprite = 2,    // linear (UNORM), no mipmaps - UI/canvas textures, exact colors
};

struct TextureAssetProperties {
  TextureAssetType asset_type = TextureAssetType::Default;
  TextureFilterMode filter_mode = TextureFilterMode::Linear;
  TextureWrapMode wrap_mode = TextureWrapMode::Repeat;
  bool generate_mipmaps = true;
  glm::vec4 slice_border = {0, 0, 0, 0};  // 9-slice (L,T,R,B) in pixels
};

// --- Font asset properties ---

enum class FontAAMode : uint8_t {
  None = 0,
  Grayscale = 1
};

struct FontAssetProperties {
  FontAAMode aa_mode = FontAAMode::Grayscale;
};

}  // namespace Wiesel