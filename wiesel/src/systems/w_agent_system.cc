//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_agent_system.h"

#include "ai/w_agent_controller.h"
#include "scene/w_scene.h"

namespace Wiesel {

void AgentSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("AgentSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  for (const auto& entity : registry.view<AgentController>()) {
    auto& controller = registry.get<AgentController>(entity);
    controller.Evaluate(delta_time);
    controller.Update(delta_time);
  }
}

}  // namespace Wiesel
