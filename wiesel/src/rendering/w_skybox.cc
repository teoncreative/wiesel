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

#include "rendering/w_skybox.h"
#include "w_engine.h"

namespace wiesel {

Skybox::Skybox(std::shared_ptr<Texture> texture) : texture_(texture) {
  descriptors_ = Engine::renderer()->CreateSkyboxDescriptors(texture_);
}

Skybox::~Skybox() {}

}  // namespace wiesel