
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

#include "rendering/w_texture.hpp"
#include "util/w_color.hpp"
#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {
static const int k_MaterialTextureCount = 7;

struct Material {
  Material();
  ~Material();

  Colorf BaseColor = {1.0f, 1.0f, 1.0f};
  Colorf DiffuseColor = {1.0f, 1.0f, 1.0f};
  Colorf SpecularColor = {1.0f, 1.0f, 1.0f};
  float Shininess = 0.5f;

  Ref<Texture> BaseTexture;
  Ref<Texture> NormalMap;
  Ref<Texture> SpecularMap;
  Ref<Texture> HeightMap;
  Ref<Texture> AlbedoMap;
  Ref<Texture> RoughnessMap;
  Ref<Texture> MetallicMap;

  static void Set(Ref<Material> material, Ref<Texture> texture,
                  TextureType type);
};
}  // namespace Wiesel