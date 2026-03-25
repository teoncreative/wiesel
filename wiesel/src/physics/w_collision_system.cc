
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "physics/w_collision_system.h"
#include "behavior/w_behavior.h"
#include "physics/w_collider.h"
#include "scene/w_components.h"
#include "script/mono/w_monobehavior.h"
#include "script/w_scriptmanager.h"

namespace Wiesel {

// Shared overlap tests
bool TestBoxBox(const ColliderEntry& a, const ColliderEntry& b) {
  glm::vec3 a_min = a.world_center - a.half_extents;
  glm::vec3 a_max = a.world_center + a.half_extents;
  glm::vec3 b_min = b.world_center - b.half_extents;
  glm::vec3 b_max = b.world_center + b.half_extents;

  return a_min.x <= b_max.x && a_max.x >= b_min.x && a_min.y <= b_max.y &&
         a_max.y >= b_min.y && a_min.z <= b_max.z && a_max.z >= b_min.z;
}

bool TestSphereSphere(const ColliderEntry& a, const ColliderEntry& b) {
  glm::vec3 diff = a.world_center - b.world_center;
  float dist_sq = glm::dot(diff, diff);
  float radius_sum = a.radius + b.radius;
  return dist_sq <= radius_sum * radius_sum;
}

bool TestBoxSphere(const ColliderEntry& box, const ColliderEntry& sphere) {
  glm::vec3 box_min = box.world_center - box.half_extents;
  glm::vec3 box_max = box.world_center + box.half_extents;
  glm::vec3 closest = glm::clamp(sphere.world_center, box_min, box_max);

  glm::vec3 diff = sphere.world_center - closest;
  float dist_sq = glm::dot(diff, diff);
  return dist_sq <= sphere.radius * sphere.radius;
}

bool TestOverlap(const ColliderEntry& a, const ColliderEntry& b) {
  if (a.shape == ColliderShape::Box && b.shape == ColliderShape::Box) {
    return TestBoxBox(a, b);
  }
  if (a.shape == ColliderShape::Sphere && b.shape == ColliderShape::Sphere) {
    return TestSphereSphere(a, b);
  }
  if (a.shape == ColliderShape::Box) {
    return TestBoxSphere(a, b);
  }
  return TestBoxSphere(b, a);
}

// Collider collection
std::vector<ColliderEntry> CollectColliderEntries(entt::registry& registry) {
  std::vector<ColliderEntry> entries;

  auto box_view = registry.view<TransformComponent, BoxColliderComponent>();
  for (entt::entity handle : box_view) {
    auto& transform = box_view.get<TransformComponent>(handle);
    auto& box = box_view.get<BoxColliderComponent>(handle);
    entries.push_back({handle, ColliderShape::Box,
                       transform.GetPosition() + box.offset, box.half_extents,
                       0.0f});
  }

  auto sphere_view =
      registry.view<TransformComponent, SphereColliderComponent>();
  for (entt::entity handle : sphere_view) {
    auto& transform = sphere_view.get<TransformComponent>(handle);
    auto& sphere = sphere_view.get<SphereColliderComponent>(handle);
    entries.push_back({handle, ColliderShape::Sphere,
                       transform.GetPosition() + sphere.offset, glm::vec3(0.0f),
                       sphere.radius});
  }

  return entries;
}

std::vector<entt::entity> QueryOverlapBox(entt::registry& registry,
                                          glm::vec3 center,
                                          glm::vec3 half_extents) {
  ColliderEntry query{entt::null, ColliderShape::Box, center, half_extents,
                      0.0f};
  auto entries = CollectColliderEntries(registry);

  std::vector<entt::entity> results;
  for (auto& entry : entries) {
    if (TestOverlap(query, entry)) {
      results.push_back(entry.entity);
    }
  }
  return results;
}

std::vector<entt::entity> QueryOverlapSphere(entt::registry& registry,
                                             glm::vec3 center, float radius) {
  ColliderEntry query{entt::null, ColliderShape::Sphere, center,
                      glm::vec3(0.0f), radius};
  auto entries = CollectColliderEntries(registry);

  std::vector<entt::entity> results;
  for (auto& entry : entries) {
    if (TestOverlap(query, entry)) {
      results.push_back(entry.entity);
    }
  }
  return results;
}

void CollisionSystem::Update(entt::registry& registry) {
  std::vector<ColliderEntry> entries = CollectColliderEntries(registry);

  // Test all pairs
  std::set<CollisionPair> current_overlaps;
  for (size_t i = 0; i < entries.size(); i++) {
    for (size_t j = i + 1; j < entries.size(); j++) {
      if (entries[i].entity == entries[j].entity) {
        continue;
      }
      if (TestOverlap(entries[i], entries[j])) {
        current_overlaps.insert(
            CollisionPair(entries[i].entity, entries[j].entity));
      }
    }
  }

  // Determine transitions
  std::set<CollisionPair> entered;
  std::set<CollisionPair> stayed;
  std::set<CollisionPair> exited;

  for (auto& pair : current_overlaps) {
    if (previous_overlaps_.contains(pair)) {
      stayed.insert(pair);
    } else {
      entered.insert(pair);
    }
  }
  for (auto& pair : previous_overlaps_) {
    if (!current_overlaps.contains(pair)) {
      exited.insert(pair);
    }
  }

  // Dispatch callbacks
  if (!entered.empty() || !stayed.empty() || !exited.empty()) {
    DispatchCallbacks(registry, entered, stayed, exited);
  }

  // Swap for next frame
  previous_overlaps_ = std::move(current_overlaps);
}

void CollisionSystem::DispatchCallbacks(entt::registry& registry,
                                        const std::set<CollisionPair>& entered,
                                        const std::set<CollisionPair>& stayed,
                                        const std::set<CollisionPair>& exited) {
  auto invoke = [&](entt::entity entity, entt::entity other,
                    void (ScriptInstance::*method)(entt::entity)) {
    if (!registry.any_of<BehaviorsComponent>(entity)) {
      return;
    }
    auto& behaviors = registry.get<BehaviorsComponent>(entity);
    for (auto& behavior : behaviors.behaviors_ | std::views::values) {
      auto* mono = dynamic_cast<MonoBehavior*>(behavior);
      if (!mono || !mono->script_instance()) {
        continue;
      }
      (mono->script_instance()->*method)(other);
    }
  };

  for (auto& pair : entered) {
    invoke(pair.a, pair.b, &ScriptInstance::OnTriggerEnter);
    invoke(pair.b, pair.a, &ScriptInstance::OnTriggerEnter);
  }
  for (auto& pair : stayed) {
    invoke(pair.a, pair.b, &ScriptInstance::OnTriggerStay);
    invoke(pair.b, pair.a, &ScriptInstance::OnTriggerStay);
  }
  for (auto& pair : exited) {
    invoke(pair.a, pair.b, &ScriptInstance::OnTriggerExit);
    invoke(pair.b, pair.a, &ScriptInstance::OnTriggerExit);
  }
}

}  // namespace Wiesel
