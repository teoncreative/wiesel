//
// Created by Metehan Gezer on 18/04/2025.
//

#include "rendering/w_skybox.hpp"
#include "w_engine.hpp"

namespace Wiesel {

Skybox::Skybox(Ref<Texture> texture) : texture_(texture) {
  descriptors_ = Engine::GetRenderer()->CreateSkyboxDescriptors(texture_);
}

Skybox::~Skybox() {

}

}