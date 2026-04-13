//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_transform_system.h"

#include "scene/w_scene.h"

namespace wiesel {

void TransformSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("TransformSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  for (const auto& entity : registry.view<TransformComponent>()) {
    auto& transform = registry.get<TransformComponent>(entity);
    if (transform.IsChanged()) {
      UpdateMatrices(scene, entity);
      transform.ClearChanged();
      MarkChildrenDirty(scene, entity);
      if (registry.any_of<CameraComponent>(entity)) {
        registry.get<CameraComponent>(entity).pos_changed = true;
      }
    }
  }
}

void TransformSystem::MarkChildrenDirty(Scene& scene, entt::entity entity) {
  entt::registry& registry = scene.GetRegistry();
  if (!registry.any_of<TreeComponent>(entity)) {
    return;
  }
  auto& tree = registry.get<TreeComponent>(entity);
  for (auto child : tree.children) {
    if (registry.any_of<TransformComponent>(child)) {
      registry.get<TransformComponent>(child).MarkChanged();
    }
    MarkChildrenDirty(scene, child);
  }
}

glm::mat4 TransformSystem::MakeLocal(const TransformComponent& t) {
  PROFILE_ZONE_SCOPED();
  glm::vec3 rot_rad = glm::radians(t.GetRotation());
  glm::mat4 R = glm::toMat4(glm::quat(rot_rad));
  glm::mat4 T = glm::translate(glm::mat4(1.0f), t.GetPosition());
  glm::mat4 Tp = glm::translate(glm::mat4(1.0f), t.GetPivot());
  glm::mat4 Tn = glm::translate(glm::mat4(1.0f), -t.GetPivot());
  glm::mat4 S = glm::scale(glm::mat4(1.0f), t.GetScale());

  // move to Position, shift to Pivot, rotate+scale, shift back
  return T * Tp * R * S * Tn;
}

glm::mat4 TransformSystem::GetWorldMatrix(Scene& scene, entt::entity entity) {
  PROFILE_ZONE_SCOPED();
  entt::registry& registry = scene.GetRegistry();
  auto& transform = registry.get<TransformComponent>(entity);
  glm::mat4 local = MakeLocal(transform);

  if (auto* tree = registry.try_get<TreeComponent>(entity);
      tree && tree->parent != entt::null) {
    return GetWorldMatrix(scene, tree->parent) * local;
  }
  return local;
}

void TransformSystem::UpdateMatrices(Scene& scene, entt::entity entity) {
  PROFILE_ZONE_SCOPED();
  auto& tc = scene.GetRegistry().get<TransformComponent>(entity);
  tc.SetTransformMatrix(GetWorldMatrix(scene, entity));
}

}  // namespace wiesel
