
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

#include <entt/entt.hpp>
#include <set>
#include <vector>
#include "w_pch.hpp"

namespace Wiesel {

// Ordered pair of entities (lower handle first) to avoid duplicates
struct CollisionPair {
  entt::entity a;
  entt::entity b;

  CollisionPair(entt::entity x, entt::entity y) {
    if (x < y) {
      a = x;
      b = y;
    } else {
      a = y;
      b = x;
    }
  }

  bool operator<(const CollisionPair& other) const {
    if (a != other.a) {
      return a < other.a;
    }
    return b < other.b;
  }

  bool operator==(const CollisionPair& other) const {
    return a == other.a && b == other.b;
  }
};

// Collider entry used during broad/narrow phase
enum class ColliderShape { Box, Sphere };

struct ColliderEntry {
  entt::entity entity;
  ColliderShape shape;
  glm::vec3 world_center;
  // Box-specific
  glm::vec3 half_extents;
  // Sphere-specific
  float radius;
};

// Shared overlap tests (used by CollisionSystem and Physics queries)
bool TestOverlap(const ColliderEntry& a, const ColliderEntry& b);
bool TestBoxBox(const ColliderEntry& a, const ColliderEntry& b);
bool TestSphereSphere(const ColliderEntry& a, const ColliderEntry& b);
bool TestBoxSphere(const ColliderEntry& box, const ColliderEntry& sphere);

// Collect all collider entries from a registry
std::vector<ColliderEntry> CollectColliderEntries(entt::registry& registry);

// Query API. Returns entity handles overlapping the given shape
std::vector<entt::entity> QueryOverlapBox(entt::registry& registry,
                                          glm::vec3 center,
                                          glm::vec3 half_extents);
std::vector<entt::entity> QueryOverlapSphere(entt::registry& registry,
                                             glm::vec3 center, float radius);

class CollisionSystem {
 public:
  void Update(entt::registry& registry);

 private:
  void DispatchCallbacks(entt::registry& registry,
                         const std::set<CollisionPair>& entered,
                         const std::set<CollisionPair>& stayed,
                         const std::set<CollisionPair>& exited);

  std::set<CollisionPair> previous_overlaps_;
};

}  // namespace Wiesel
