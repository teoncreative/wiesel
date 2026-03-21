
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
#include <vector>

namespace Wiesel {

enum CollisionGroup : uint16_t {
  CollisionGroupDefault   = 1 << 0,
  CollisionGroupTerrain   = 1 << 1,
  CollisionGroupBuilding  = 1 << 2,
  CollisionGroupCharacter = 1 << 3,
  CollisionGroupAll       = 0xFFFF,
};

struct BoxColliderComponent : public IComponent {
  BoxColliderComponent() = default;
  BoxColliderComponent(const BoxColliderComponent&) = default;

  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  glm::vec3 half_extents = {0.5f, 0.5f, 0.5f};
  bool is_trigger = false;
  uint16_t collision_group = CollisionGroupDefault;
};

struct SphereColliderComponent : public IComponent {
  SphereColliderComponent() = default;
  SphereColliderComponent(const SphereColliderComponent&) = default;

  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  float radius = 0.5f;
  bool is_trigger = false;
  uint16_t collision_group = CollisionGroupDefault;
};

enum class CapsuleAxis : int {
  X = 0,
  Y = 1,
  Z = 2,
};

struct CapsuleColliderComponent : public IComponent {
  CapsuleColliderComponent() = default;
  CapsuleColliderComponent(const CapsuleColliderComponent&) = default;

  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  float radius = 0.3f;
  float height = 1.0f;   // height of the cylindrical section (total = height + 2*radius)
  CapsuleAxis axis = CapsuleAxis::Y;
  bool is_trigger = false;
  uint16_t collision_group = CollisionGroupDefault;
};

struct HeightfieldColliderComponent : public IComponent {
  HeightfieldColliderComponent() = default;
  HeightfieldColliderComponent(const HeightfieldColliderComponent&) = default;

  int width = 0;
  int length = 0;
  std::vector<float> height_data;  // row-major, owned here (Bullet reads pointer)
  float min_height = 0.0f;
  float max_height = 1.0f;
  glm::vec3 scale = {1.0f, 1.0f, 1.0f};  // XZ per cell + Y multiplier
  uint16_t collision_group = CollisionGroupTerrain;
};

}  // namespace Wiesel
