//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 24.03.2026.
//

#include "asset/w_asset_utils.h"

namespace Wiesel {

AssetType ExtToAssetType(const std::string& ext) {
  if (ext == ".wscene") {
    return AssetType::Scene;
  }
  if (ext == ".wprefab") {
    return AssetType::Prefab;
  }
  if (ext == ".wmat") {
    return AssetType::Material;
  }
  if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") {
    return AssetType::Model;
  }
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
      ext == ".bmp") {
    return AssetType::Texture;
  }
  if (ext == ".ttf" || ext == ".otf") {
    return AssetType::Font;
  }
  if (ext == ".cs") {
    return AssetType::Script;
  }
  if (ext == ".wskybox") {
    return AssetType::Skybox;
  }
  if (ext == ".wsprite") {
    return AssetType::Sprite;
  }
  if (ext == ".wanimclip") {
    return AssetType::AnimClip;
  }
  if (ext == ".wanimcontroller") {
    return AssetType::AnimController;
  }
  if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
    return AssetType::Audio;
  }
  if (ext == ".rml") {
    return AssetType::UIDocument;
  }
  if (ext == ".rcss") {
    return AssetType::UIStylesheet;
  }
  if (ext == ".wcursorset") {
    return AssetType::CursorSet;
  }
  if (ext == ".wmeshcol") {
    return AssetType::MeshCollider;
  }
  return AssetType::None;
}

bool IsJsonAssetType(AssetType type) {
  return type == AssetType::Scene || type == AssetType::Prefab ||
         type == AssetType::Material || type == AssetType::Skybox ||
         type == AssetType::Sprite || type == AssetType::AnimClip ||
         type == AssetType::AnimController || type == AssetType::CursorSet ||
         type == AssetType::MeshCollider;
}

}  // namespace Wiesel
