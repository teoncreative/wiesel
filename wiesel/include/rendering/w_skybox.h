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

#include "rendering/w_texture.h"
#include "util/w_utils.h"
#include "w_pch.h"

namespace Wiesel {

class Skybox {
 public:
  Skybox(std::shared_ptr<Texture> texture);
  ~Skybox();

  std::shared_ptr<Texture> texture_;
  std::shared_ptr<DescriptorSet> descriptors_;
};

}  // namespace Wiesel

#endif  //WIESEL_SKYBOX_HPP
