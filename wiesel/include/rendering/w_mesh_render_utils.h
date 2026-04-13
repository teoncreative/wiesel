
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
#include "asset/w_asset_manager.h"
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_renderer.h"
#include "scene/w_components.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace wiesel {

// Resolve the skeleton root's transform and SkeletalAnimRuntime for a
// skinned mesh renderer. Returns false if the skeleton root is invalid.
inline bool ResolveSkeletonRoot(Scene& scene,
                                const SkinnedMeshRendererComponent& mr,
                                const TransformComponent*& out_transform,
                                const SkeletalAnimRuntime*& out_skel) {
  out_skel = nullptr;
  if (mr.skeleton_root == entt::null ||
      !scene.GetRegistry().valid(mr.skeleton_root)) {
    return false;
  }
  if (scene.GetRegistry().all_of<TransformComponent>(mr.skeleton_root)) {
    out_transform =
        &scene.GetRegistry().get<TransformComponent>(mr.skeleton_root);
  }
  if (scene.GetRegistry().all_of<SkeletalAnimRuntime>(mr.skeleton_root)) {
    out_skel = &scene.GetRegistry().get<SkeletalAnimRuntime>(mr.skeleton_root);
  }
  return true;
}

// Check if a mesh's material is double-sided.
inline bool IsMeshDoubleSided(AssetHandle model_handle, int32_t mesh_index) {
  auto model_data = Engine::asset_manager().Get<Model>(model_handle);
  if (model_data && mesh_index >= 0 &&
      mesh_index < static_cast<int32_t>(model_data->meshes.size())) {
    auto& mat = model_data->meshes[mesh_index]->mat;
    return mat && mat->double_sided;
  }
  return false;
}

// Frustum cull a static mesh. Returns true if the mesh should be culled.
inline bool FrustumCullMesh(const FrustumPlanes& frustum,
                            AssetHandle model_handle, int32_t mesh_index,
                            const glm::mat4& world_transform) {
  auto model_data = Engine::asset_manager().Get<Model>(model_handle);
  if (model_data && mesh_index >= 0 &&
      mesh_index < static_cast<int32_t>(model_data->meshes.size())) {
    AABB world_bounds =
        model_data->meshes[mesh_index]->bounds.Transformed(world_transform);
    return frustum.IsBoxOutside(world_bounds.min, world_bounds.max);
  }
  return false;
}

// Frustum cull a skinned mesh. Returns true if the mesh should be culled.
inline bool FrustumCullSkinned(const FrustumPlanes& frustum,
                               const SkeletalAnimRuntime* skel,
                               const glm::mat4& world_transform) {
  if (skel && skel->rest_pose_bounds.Valid()) {
    AABB world_bounds = skel->rest_pose_bounds.Transformed(world_transform);
    if (skel->max_bone_reach > 0.0f) {
      glm::vec3 expand(skel->max_bone_reach);
      world_bounds.min -= expand;
      world_bounds.max += expand;
    }
    return frustum.IsBoxOutside(world_bounds.min, world_bounds.max);
  }
  return false;
}

}  // namespace wiesel
