//
// Created by Metehan Gezer on 18/04/2025.
//

#include "rendering/w_skybox.hpp"
#include "w_engine.hpp"

namespace Wiesel {

Skybox::Skybox(Ref<Texture> texture) : m_Texture(texture) {
  m_Descriptors = Engine::GetRenderer()->CreateSkyboxDescriptors(m_Texture);
}

Skybox::~Skybox() {

}

}