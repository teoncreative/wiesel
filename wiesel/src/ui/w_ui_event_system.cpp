
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

  // Transform mouse from viewport-pixel space to canvas space.
  // mouse_x/y are already relative to the viewport origin.
  // Step 1: viewport display pixels -> render resolution pixels
  // Step 2: render resolution pixels -> canvas reference resolution
  glm::vec2 display_size = scene.GetViewportDisplaySize();
  glm::vec2 render_res = scene.GetRenderResolution();

  // If render_res is zero (Free Aspect), use display size as render res
  if (render_res.x <= 0 || render_res.y <= 0) {
    render_res = display_size;
  }

  float canvas_mx = mouse_x;
  float canvas_my = mouse_y;

  // Scale from display pixels to render pixels
  if (display_size.x > 0 && display_size.y > 0 &&
      render_res.x > 0 && render_res.y > 0) {
    canvas_mx = mouse_x * (render_res.x / display_size.x);
    canvas_my = mouse_y * (render_res.y / display_size.y);
  }

  // Scale from render pixels to canvas reference resolution
  for (auto e : registry.view<CanvasComponent, CanvasScalerComponent>()) {
    auto& scaler = registry.get<CanvasScalerComponent>(e);
    if (scaler.scale_mode == ScaleMode::ScaleWithScreenSize &&
        render_res.x > 0 && render_res.y > 0) {
      float scale_w = render_res.x / scaler.reference_resolution.x;
      float scale_h = render_res.y / scaler.reference_resolution.y;
      float t = scaler.match_width_or_height;
      float scale_factor = scale_w * (1.0f - t) + scale_h * t;
      canvas_mx /= scale_factor;
      canvas_my /= scale_factor;
      break;
    }
  }

  // Collect all interactable entities sorted by draw order (front to back)
  struct HitCandidate {
    entt::entity entity;
    int32_t draw_order;
  };
  std::vector<HitCandidate> candidates;

  for (auto entity : registry.view<InteractableComponent, RectangleTransformComponent>()) {
    auto& interactable = registry.get<InteractableComponent>(entity);
    if (!interactable.enabled) {
      continue;
    }

    auto& rt = registry.get<RectangleTransformComponent>(entity);
    if (PointInRect(canvas_mx, canvas_my, rt.computed_position, rt.computed_size)) {
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

  // Update ButtonComponent state from InteractableComponent
  for (auto entity : registry.view<ButtonComponent, InteractableComponent>()) {
    auto& btn = registry.get<ButtonComponent>(entity);
    auto& interactable = registry.get<InteractableComponent>(entity);

    if (!interactable.enabled) {
      btn.state_ = ButtonState::Disabled;
    } else if (interactable.pressed_) {
      btn.state_ = ButtonState::Pressed;
    } else if (interactable.hovered_) {
      btn.state_ = ButtonState::Hovered;
    } else {
      btn.state_ = ButtonState::Normal;
    }
  }
}

}  // namespace Wiesel