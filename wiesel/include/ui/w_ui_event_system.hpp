
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
#include <glm/glm.hpp>
#include <map>

namespace Wiesel {

class Scene;
class IBehavior;

// Per-player navigation state for gamepad-driven UI.
struct PlayerNavState {
  entt::entity selected_entity = entt::null;
  float nav_repeat_timer = 0.0f;
  bool nav_held = false;
};

class UIEventSystem {
 public:
  void Update(Scene& scene, float delta_time);

  entt::entity GetFocusedEntity() const { return focused_entity_; }

  void SetFocusedEntity(entt::entity entity) { focused_entity_ = entity; }

  void ClearFocus() { focused_entity_ = entt::null; }

  entt::entity GetSelectedEntity(int player_index = 0) const;

 private:
  // Mouse state
  entt::entity hovered_entity_ = entt::null;
  entt::entity pressed_entity_ = entt::null;
  entt::entity focused_entity_ = entt::null;
  bool mouse_dirty_ = true;
  float last_mouse_x_ = -1.0f;
  float last_mouse_y_ = -1.0f;

  // Per-player gamepad navigation state
  std::map<int, PlayerNavState> player_nav_;

  // Navigation repeat timing config
  static constexpr float kNavRepeatDelay = 0.4f;
  static constexpr float kNavRepeatRate = 0.12f;

  void ProcessMouseInput(Scene& scene);
  void ProcessGamepadInput(Scene& scene, float delta_time, int player_index,
                           entt::entity canvas_entity);
  void ClearSelection(Scene& scene, int player_index);
  void NavigateTo(Scene& scene, int player_index, entt::entity target);
  entt::entity FindNeighbor(Scene& scene, entt::entity from,
                            glm::vec2 direction, entt::entity canvas_root);
  entt::entity FindFirstNavigable(Scene& scene, entt::entity canvas_root);
  void UpdateButtonStates(entt::registry& registry);
  void FocusEntity(entt::registry& registry, entt::entity entity);
  void UnfocusEntity(entt::registry& registry);

  template <typename Fn>
  entt::entity DispatchWithBubble(entt::registry& registry, entt::entity start,
                                  Fn&& fn);
};

}  // namespace Wiesel