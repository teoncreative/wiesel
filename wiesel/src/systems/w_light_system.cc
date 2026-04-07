//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_light_system.h"

#include "rendering/w_renderer.h"
#include "scene/w_lights.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace Wiesel {

void LightSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("LightSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  auto& lights = Engine::renderer()->lights_data();
  lights.direct_light_count = 0;
  lights.point_light_count = 0;
  for (const auto& entity : registry.view<LightDirectComponent>()) {
    auto& light = registry.get<LightDirectComponent>(entity);
    auto& transform = registry.get<TransformComponent>(entity);
    UpdateLight(lights, light.light_data, transform);
  }
  for (const auto& entity : registry.view<LightPointComponent>()) {
    auto& light = registry.get<LightPointComponent>(entity);
    auto& transform = registry.get<TransformComponent>(entity);
    UpdateLight(lights, light.light_data, transform);
  }
}

}  // namespace Wiesel
