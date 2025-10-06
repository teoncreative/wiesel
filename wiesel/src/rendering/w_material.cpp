
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_material.hpp"

namespace Wiesel {

Material::Material() {}

Material::~Material() {
  base_texture = nullptr;
  normal_map = nullptr;
  specular_map = nullptr;
  height_map = nullptr;
  albedo_map = nullptr;
  roughness_map = nullptr;
  metallic_map = nullptr;
}

void Material::Set(Ref<Material> material, Ref<Texture> texture,
                   TextureType type) {
  switch (type) {
    case TextureTypeNone:
      break;
    case TextureTypeDiffuse:
      material->base_texture = texture;
      break;
    case TextureTypeSpecular:
      material->specular_map = texture;
      break;
    case TextureTypeAmbient:
      break;
    case TextureTypeEmissive:
      break;
    case TextureTypeHeight:
      material->height_map = texture;
      break;
    case TextureTypeNormals:
      material->normal_map = texture;
      break;
    case TextureTypeShininess:
      break;
    case TextureTypeOpacty:
      break;
    case TextureTypeDisplacement:
      break;
    case TextureTypeLightmap:
      break;
    case TextureTypeReflection:
      break;
    case TextureTypeBaseColor:
      material->albedo_map = texture;
      break;
    case TextureTypeNormalCamera:
      material->normal_map = texture;
      break;
    case TextureTypeEmissionColor:
      break;
    case TextureTypeMetalness:
      material->metallic_map = texture;
      break;
    case TextureTypeDiffuseRoughness:
      material->roughness_map = texture;
      break;
    case TextureTypeAmbientOcclusion:
      break;
    case TextureTypeSheen:
      break;
    case TextureTypeClearcoat:
      break;
    case TextureTypeTransmission:
      break;
  }
}

}  // namespace Wiesel