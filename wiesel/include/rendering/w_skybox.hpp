//
// Created by Metehan Gezer on 18/04/2025.
//

#ifndef WIESEL_SKYBOX_HPP
#define WIESEL_SKYBOX_HPP

#include "util/w_utils.hpp"
#include "w_pch.hpp"
#include "rendering/w_texture.hpp"

namespace Wiesel {

class Skybox {
 public:
  Skybox(Ref<Texture> texture);
  ~Skybox();

  Ref<Texture> m_Texture;
  Ref<DescriptorData> m_Descriptors;
};

}

#endif  //WIESEL_SKYBOX_HPP
