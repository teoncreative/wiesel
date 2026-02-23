
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

#include "scene/w_components.hpp"

namespace Wiesel {

struct BoxColliderComponent : public IComponent {
  BoxColliderComponent() = default;
  BoxColliderComponent(const BoxColliderComponent&) = default;

  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  glm::vec3 half_extents = {0.5f, 0.5f, 0.5f};
  bool is_trigger = false;
};

struct SphereColliderComponent : public IComponent {
  SphereColliderComponent() = default;
  SphereColliderComponent(const SphereColliderComponent&) = default;

  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  float radius = 0.5f;
  bool is_trigger = false;
};

}  // namespace Wiesel
