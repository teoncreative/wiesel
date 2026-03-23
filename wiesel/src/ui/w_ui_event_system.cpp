
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
#include "rendering/w_camera.hpp"
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

  // Build a camera ray for world-space canvas hit testing.
  // Use viewport-relative mouse position (before canvas scaler transform)
  // scaled to render resolution for correct NDC.
  auto camera_data = Engine::renderer()->GetCameraData();
  glm::vec3 ray_origin{0};
  glm::vec3 ray_dir{0, 0, -1};
  bool has_ray = false;

  if (camera_data && display_size.x > 0 && display_size.y > 0) {
    // Convert viewport mouse position to NDC.
    // Vulkan Y flip is baked into the projection matrix, so don't flip Y here.
    float ndc_x = (mouse_x / display_size.x) * 2.0f - 1.0f;
    float ndc_y = (mouse_y / display_size.y) * 2.0f - 1.0f;

    glm::mat4 inv_proj = glm::inverse(camera_data->projection);
    glm::mat4 inv_view = glm::inverse(camera_data->view_matrix);

    // Unproject near and far points
    glm::vec4 near_ndc = {ndc_x, ndc_y, -1.0f, 1.0f};
    glm::vec4 far_ndc = {ndc_x, ndc_y, 1.0f, 1.0f};

    glm::vec4 near_view = inv_proj * near_ndc;
    near_view /= near_view.w;
    glm::vec4 far_view = inv_proj * far_ndc;
    far_view /= far_view.w;

    glm::vec3 near_world = glm::vec3(inv_view * near_view);
    glm::vec3 far_world = glm::vec3(inv_view * far_view);

    ray_origin = near_world;
    ray_dir = glm::normalize(far_world - near_world);
    has_ray = true;
  }

  // Helper: find canvas root for an entity
  auto find_canvas_root = [&registry](entt::entity entity) -> entt::entity {
    entt::entity walk = entity;
    while (walk != entt::null) {
      if (registry.any_of<CanvasComponent>(walk)) {
        return walk;
      }
      if (registry.any_of<TreeComponent>(walk)) {
        walk = registry.get<TreeComponent>(walk).parent;
      } else {
        break;
      }
    }
    return entt::null;
  };

  for (auto entity : registry.view<InteractableComponent, RectangleTransformComponent>()) {
    auto& interactable = registry.get<InteractableComponent>(entity);
    if (!interactable.enabled) {
      continue;
    }

    entt::entity canvas_root = find_canvas_root(entity);
    if (canvas_root == entt::null) {
      continue;
    }

    auto& canvas = registry.get<CanvasComponent>(canvas_root);
    auto& rt = registry.get<RectangleTransformComponent>(entity);

    if (canvas.render_mode == CanvasRenderMode::WorldSpace) {
      // World-space: raycast against the canvas plane
      if (!has_ray || !registry.any_of<TransformComponent>(canvas_root)) {
        continue;
      }

      auto& canvas_transform = registry.get<TransformComponent>(canvas_root);
      glm::mat4 model = canvas_transform.GetTransformMatrix();
      glm::vec3 plane_normal = glm::normalize(glm::vec3(model[2]));
      glm::vec3 plane_origin = glm::vec3(model[3]);

      // Ray-plane intersection
      float denom = glm::dot(plane_normal, ray_dir);
      if (std::abs(denom) < 1e-6f) {
        continue;
      }
      float t = glm::dot(plane_origin - ray_origin, plane_normal) / denom;
      if (t < 0) {
        continue;
      }

      glm::vec3 hit_world = ray_origin + ray_dir * t;

      // Convert hit point to canvas pixel coordinates
      glm::mat4 inv_model = glm::inverse(model);
      glm::vec3 hit_local = glm::vec3(inv_model * glm::vec4(hit_world, 1.0f));

      // Get canvas reference resolution and pixels_per_unit from scaler
      glm::vec2 ref_size = {1920, 1080};
      float ppu = 100.0f;
      if (registry.any_of<CanvasScalerComponent>(canvas_root)) {
        auto& scaler = registry.get<CanvasScalerComponent>(canvas_root);
        ref_size = scaler.reference_resolution;
        ppu = std::max(1.0f, scaler.reference_pixels_per_unit);
      }
      glm::vec2 ws = ref_size / ppu;

      // Reverse the shader transform: localWorld → normalizedPos → pixelPos
      // Shader: normalizedPos = (pixelPos / canvasSize) - 0.5; normalizedPos.y = -normalizedPos.y;
      // Reverse: normalizedPos = localWorld.xy / worldSize; undo Y flip; pixelPos = (norm + 0.5) * canvasSize
      glm::vec2 normalized = glm::vec2(hit_local.x, hit_local.y) / ws;
      normalized.y = -normalized.y;  // undo the shader's Y flip
      glm::vec2 canvas_pos = (normalized + glm::vec2(0.5f)) * ref_size;

      if (PointInRect(canvas_pos.x, canvas_pos.y, rt.computed_position, rt.computed_size)) {
        candidates.push_back({entity, rt.draw_order});
      }
    } else {
      // Screen-space (Overlay and ScreenSpaceCamera): 2D hit test
      if (PointInRect(canvas_mx, canvas_my, rt.computed_position, rt.computed_size)) {
        candidates.push_back({entity, rt.draw_order});
      }
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