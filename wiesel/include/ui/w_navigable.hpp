//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <entt/entt.hpp>
#include "scene/w_components.hpp"

namespace Wiesel {

// Marks a UI element as navigable by gamepad/keyboard.
// Requires InteractableComponent for event handling.
// Neighbors can be set explicitly or left as entt::null for auto-navigation.
struct NavigableComponent : public IComponent {
  entt::entity nav_up = entt::null;
  entt::entity nav_down = entt::null;
  entt::entity nav_left = entt::null;
  entt::entity nav_right = entt::null;
};

}  // namespace Wiesel