//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_skinned_mesh_system.h"

#include "rendering/w_mesh_renderer.h"
#include "scene/w_scene.h"

namespace wiesel {

void SkinnedMeshSystem::Update(Scene& scene, float /*delta_time*/) {
  PROFILE_ZONE_SCOPED_N("SkinnedMeshSystem::Update");
  entt::registry& registry = scene.GetRegistry();

  // For every SkinnedMeshRendererComponent, ensure its skeleton_root
  // has a properly initialized SkeletalAnimRuntime.
  for (auto entity : registry.view<SkinnedMeshRendererComponent>()) {
    auto& mr = registry.get<SkinnedMeshRendererComponent>(entity);
    if (mr.skeleton_root == entt::null || !registry.valid(mr.skeleton_root)) {
      continue;
    }

    if (!registry.all_of<SkeletalAnimRuntime>(mr.skeleton_root)) {
      auto& skel = registry.emplace<SkeletalAnimRuntime>(mr.skeleton_root);
      skel.model_handle = mr.model_handle;
      skel.Initialize();
    } else {
      auto& skel = registry.get<SkeletalAnimRuntime>(mr.skeleton_root);
      if (!skel.initialized) {
        skel.Initialize();
      }
    }
  }
}

}  // namespace wiesel
