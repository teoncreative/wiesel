//
// Created by Metehan Gezer on 18/04/2025.
//

#ifndef WIESEL_SKYBOX_HPP
#define WIESEL_SKYBOX_HPP

#include "rendering/w_texture.hpp"
#include "util/w_utils.hpp"
#include "w_pch.hpp"

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
