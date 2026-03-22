
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_event_system.hpp"

#include "asset/w_asset_manager.hpp"
#include "behavior/w_behavior.hpp"
#include "scene/w_components.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "ui/w_canvas.hpp"
#include "ui/w_interactable.hpp"
#include "util/w_logger.hpp"
#include "w_engine.hpp"

namespace Wiesel {

static bool PointInRect(float px, float py, const glm::vec2& pos, const glm::vec2& size) {
  return px >= pos.x && px <= pos.x + size.x &&
         py >= pos.y && py <= pos.y + size.y;
}

void UIEventSystem::Update(Scene& scene, float mouse_x, float mouse_y,
                            bool mouse_down, bool mouse_up, bool mouse_held) {
  auto& registry = scene.GetRegistry();

  // Collect all interactable entities sorted by draw order (front to back)
  struct HitCandidate {
    entt::entity entity;
    int32_t draw_order;
  };
  std::vector<HitCandidate> candidates;

  for (auto entity : registry.view<InteractableComponent, RectangleTransformComponent>()) {
    auto& interactable = registry.get<InteractableComponent>(entity);
    if (!interactable.enabled) continue;

    auto& rt = registry.get<RectangleTransformComponent>(entity);
    if (PointInRect(mouse_x, mouse_y, rt.computed_position, rt.computed_size)) {
      candidates.push_back({entity, rt.draw_order});
    }
  }

  // Sort by draw order descending (highest = frontmost = checked first)
  std::sort(candidates.begin(), candidates.end(),
            [](const HitCandidate& a, const HitCandidate& b) {
              return a.draw_order > b.draw_order;
            });

  // Find the topmost hit entity
  entt::entity hit_entity = entt::null;
  if (!candidates.empty()) {
    hit_entity = candidates[0].entity;
  }

  // Hover enter/exit
  if (hit_entity != hovered_entity_) {
    // Exit old
    if (hovered_entity_ != entt::null && registry.valid(hovered_entity_)) {
      auto& interactable = registry.get<InteractableComponent>(hovered_entity_);
      interactable.hovered_ = false;

      if (registry.any_of<BehaviorsComponent>(hovered_entity_)) {
        auto& bc = registry.get<BehaviorsComponent>(hovered_entity_);
        for (auto& [name, behavior] : bc.behaviors_) {
          behavior->OnPointerExit();
        }
      }
    }

    // Enter new
    if (hit_entity != entt::null) {
      auto& interactable = registry.get<InteractableComponent>(hit_entity);
      interactable.hovered_ = true;

      if (registry.any_of<BehaviorsComponent>(hit_entity)) {
        auto& bc = registry.get<BehaviorsComponent>(hit_entity);
        for (auto& [name, behavior] : bc.behaviors_) {
          behavior->OnPointerEnter();
        }
      }
    }

    hovered_entity_ = hit_entity;
  }

  // Pointer down
  if (mouse_down) {
    // Update text input focus
    entt::entity new_focus = entt::null;
    if (hit_entity != entt::null && registry.any_of<TextInputComponent>(hit_entity)) {
      new_focus = hit_entity;
    }
    if (new_focus != focused_entity_) {
      // Unfocus old
      if (focused_entity_ != entt::null && registry.valid(focused_entity_)
          && registry.any_of<TextInputComponent>(focused_entity_)) {
        registry.get<TextInputComponent>(focused_entity_).focused_ = false;
      }
      // Focus new
      if (new_focus != entt::null) {
        auto& input = registry.get<TextInputComponent>(new_focus);
        input.focused_ = true;
        input.cursor_pos_ = static_cast<int>(input.text.size());
        input.cursor_visible_ = true;
        input.cursor_timer_ = 0.0f;
      }
      focused_entity_ = new_focus;
    }

    if (hit_entity != entt::null) {
      auto& interactable = registry.get<InteractableComponent>(hit_entity);
      interactable.pressed_ = true;
      pressed_entity_ = hit_entity;

      if (registry.any_of<BehaviorsComponent>(hit_entity)) {
        auto& bc = registry.get<BehaviorsComponent>(hit_entity);
        for (auto& [name, behavior] : bc.behaviors_) {
          if (behavior->OnPointerDown(mouse_x, mouse_y)) {
            break;
          }
        }
      }
    }
  }

  // Pointer up
  if (mouse_up) {
    if (pressed_entity_ != entt::null && registry.valid(pressed_entity_)) {
      auto& interactable = registry.get<InteractableComponent>(pressed_entity_);
      interactable.pressed_ = false;

      // Click = pressed and released on the same entity
      if (pressed_entity_ == hit_entity) {
        if (registry.any_of<BehaviorsComponent>(pressed_entity_)) {
          auto& bc = registry.get<BehaviorsComponent>(pressed_entity_);
          for (auto& [name, behavior] : bc.behaviors_) {
            if (behavior->OnPointerClick(mouse_x, mouse_y)) break;
          }
        }
      }

      if (registry.any_of<BehaviorsComponent>(pressed_entity_)) {
        auto& bc = registry.get<BehaviorsComponent>(pressed_entity_);
        for (auto& [name, behavior] : bc.behaviors_) {
          if (behavior->OnPointerUp(mouse_x, mouse_y)) break;
        }
      }
    }
    pressed_entity_ = entt::null;
  }

  // Update ButtonComponent visual states
  for (auto entity : registry.view<ButtonComponent, InteractableComponent>()) {
    auto& btn = registry.get<ButtonComponent>(entity);
    auto& interactable = registry.get<InteractableComponent>(entity);

    // Determine state
    ButtonState new_state;
    if (!interactable.enabled) {
      new_state = ButtonState::Disabled;
    } else if (interactable.pressed_) {
      new_state = ButtonState::Pressed;
    } else if (interactable.hovered_) {
      new_state = ButtonState::Hovered;
    } else {
      new_state = ButtonState::Normal;
    }

    if (new_state != btn.state_) {
      btn.state_ = new_state;

      // Pick color and texture for current state
      glm::vec4 color;
      AssetHandle tex_handle;
      switch (btn.state_) {
        case ButtonState::Hovered:
          color = btn.hovered_color;
          tex_handle = btn.hovered_texture;
          break;
        case ButtonState::Pressed:
          color = btn.pressed_color;
          tex_handle = btn.pressed_texture;
          break;
        case ButtonState::Disabled:
          color = btn.disabled_color;
          tex_handle = btn.disabled_texture;
          break;
        default:
          color = btn.normal_color;
          tex_handle = btn.normal_texture;
          break;
      }

      // Apply color to sibling CanvasImage or CanvasRect
      if (registry.any_of<CanvasImageComponent>(entity)) {
        auto& img = registry.get<CanvasImageComponent>(entity);
        img.tint = color;

        // Swap texture if one is set for this state
        if (tex_handle.IsValid()) {
          auto tex = Engine::asset_manager().GetOrLoad<Texture>(tex_handle);
          if (tex) {
            img.texture = tex;
            img.gpu_dirty_ = true;
          }
        }
      } else if (registry.any_of<CanvasRectComponent>(entity)) {
        registry.get<CanvasRectComponent>(entity).color = color;
      }
    }
  }
}

}  // namespace Wiesel