
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

namespace Wiesel {

class Scene;

// Processes mouse input against InteractableComponents each frame.
// Dispatches OnPointerClick/Down/Up/Enter/Exit to scripts on hit entities.
class UIEventSystem {
 public:
  void Update(Scene& scene, float mouse_x, float mouse_y,
              bool mouse_down, bool mouse_up, bool mouse_held);

  // Text input focus
  entt::entity GetFocusedEntity() const { return focused_entity_; }
  void SetFocusedEntity(entt::entity entity) { focused_entity_ = entity; }
  void ClearFocus() { focused_entity_ = entt::null; }

 private:
  entt::entity hovered_entity_ = entt::null;
  entt::entity pressed_entity_ = entt::null;
  entt::entity focused_entity_ = entt::null;
};

}  // namespace Wiesel