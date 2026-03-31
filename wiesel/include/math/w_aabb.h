//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_pch.h"

namespace Wiesel {

struct AABB {
  glm::vec3 min{std::numeric_limits<float>::max()};
  glm::vec3 max{std::numeric_limits<float>::lowest()};

  void Expand(const glm::vec3& point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
  }

  void Expand(const AABB& other) {
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
  }

  glm::vec3 Center() const { return (min + max) * 0.5f; }

  glm::vec3 Extents() const { return (max - min) * 0.5f; }

  bool Valid() const {
    return min.x <= max.x && min.y <= max.y && min.z <= max.z;
  }

  // Transform local-space AABB to world-space AABB
  AABB Transformed(const glm::mat4& m) const {
    glm::vec3 center = Center();
    glm::vec3 extents = Extents();
    glm::vec3 new_center = glm::vec3(m * glm::vec4(center, 1.0f));
    glm::vec3 new_extents(
        std::abs(m[0][0]) * extents.x + std::abs(m[1][0]) * extents.y +
            std::abs(m[2][0]) * extents.z,
        std::abs(m[0][1]) * extents.x + std::abs(m[1][1]) * extents.y +
            std::abs(m[2][1]) * extents.z,
        std::abs(m[0][2]) * extents.x + std::abs(m[1][2]) * extents.y +
            std::abs(m[2][2]) * extents.z);
    return {new_center - new_extents, new_center + new_extents};
  }
};

}  // namespace Wiesel
