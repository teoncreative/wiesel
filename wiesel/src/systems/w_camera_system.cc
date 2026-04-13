//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_camera_system.h"

#include "rendering/w_renderer.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace wiesel {

void CameraSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("CameraSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  auto& lights = Engine::renderer()->lights_data();
  for (const auto& entity :
       registry.view<CameraComponent, TransformComponent>()) {
    auto& camera = registry.get<CameraComponent>(entity);
    auto& transform = registry.get<TransformComponent>(entity);
    if (!camera.enabled) {
      continue;
    }
    if (camera.view_changed) {
      camera.UpdateProjection();
      camera.view_changed = false;
    }
    if (camera.pos_changed) {
      // Strip scale from the world transform - cameras should not be
      // affected by entity scale (frustum, view matrix, etc.)
      glm::mat4 world = transform.GetTransformMatrix();
      glm::vec3 ws = transform.GetWorldScale();
      if (ws.x != 0.0f && ws.y != 0.0f && ws.z != 0.0f) {
        world[0] /= ws.x;
        world[1] /= ws.y;
        world[2] /= ws.z;
      }
      camera.UpdateView(world);
      camera.pos_changed = false;
    }
    if (camera.any_changed) {
      camera.UpdateAll();
      camera.any_changed = false;
    }
    if (lights.direct_light_count > 0 &&
        Engine::renderer()->options().shadows_enabled) {
      camera.ComputeCascades(
          -glm::normalize(lights.direct_lights[0].direction));
    } else {
      camera.does_shadow_pass = false;
    }
  }
}

}  // namespace wiesel
