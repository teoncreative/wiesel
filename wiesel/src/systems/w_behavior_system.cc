//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_behavior_system.h"

#include <ranges>

#include "behavior/w_behavior.h"
#include "scene/w_scene.h"

namespace wiesel {

void BehaviorSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("BehaviorSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  for (const auto& entity : registry.view<BehaviorsComponent>()) {
    BehaviorsComponent& component = registry.get<BehaviorsComponent>(entity);
    for (IBehavior*& value : component.behaviors_ | std::views::values) {
      value->OnUpdate(delta_time);
    }
  }
}

}  // namespace wiesel
