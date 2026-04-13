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
// Created by Metehan Gezer on 18/04/2025.
//

#ifndef WIESEL_SKYBOX_HPP
#define WIESEL_SKYBOX_HPP

#include <array>
#include "asset/w_asset_handle.h"
#include "rendering/w_texture.h"
#include "util/w_utils.h"
#include "w_pch.h"

namespace wiesel {

enum class SkyboxType { Panorama, Cubemap, Cross };

struct SkyboxAssetData {
  SkyboxType type = SkyboxType::Panorama;
  AssetHandle source_handle;  // for panorama/cross types
  std::array<AssetHandle, 6>
      face_handles;  // for cubemap: right, left, top, bottom, front, back
};

class Skybox {
 public:
  Skybox(std::shared_ptr<Texture> texture);
  ~Skybox();

  std::shared_ptr<Texture> texture_;
  std::shared_ptr<DescriptorSet> descriptors_;
};

}  // namespace wiesel

#endif  //WIESEL_SKYBOX_HPP
