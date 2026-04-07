//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_text_input_system.h"

#include "scene/w_scene.h"
#include "ui/w_canvas.h"

namespace Wiesel {

void TextInputSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("TextInputSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  UIEventSystem& ui_events = scene.GetUIEventSystem();

  // Cursor blink for focused text input
  entt::entity focused = ui_events.GetFocusedEntity();
  if (focused != entt::null && registry.valid(focused) &&
      registry.any_of<TextInputComponent>(focused)) {
    auto& input = registry.get<TextInputComponent>(focused);
    input.cursor_timer_ += delta_time;
    if (input.cursor_timer_ >= 0.5f) {
      input.cursor_visible_ = !input.cursor_visible_;
      input.cursor_timer_ = 0.0f;
    }
  }

  // Sync all TextInput text to sibling TextComponent
  for (auto entity : registry.view<TextInputComponent, TextComponent>()) {
    auto& input = registry.get<TextInputComponent>(entity);
    auto& text = registry.get<TextComponent>(entity);
    if (input.text.empty() && !input.focused_) {
      text.text = input.placeholder;
      text.color = input.placeholder_color;
    } else {
      text.text = input.text;
    }
  }
}

}  // namespace Wiesel
