
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_event_system.h"

#include "RmlUi/Core/Context.h"
#include "asset/w_asset_manager.h"
#include "behavior/w_behavior.h"
#include "input/w_input.h"
#include "rendering/w_camera.h"
#include "scene/w_components.h"
#include "ui/w_canvas.h"
#include "ui/w_interactable.h"
#include "ui/w_navigable.h"
#include "ui/w_ui_document.h"
#include "util/w_logger.h"
#include "w_engine.h"

namespace Wiesel {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool PointInRect(float px, float py, const glm::vec2& pos,
                        const glm::vec2& size) {
  return px >= pos.x && px <= pos.x + size.x && py >= pos.y &&
         py <= pos.y + size.y;
}

static glm::vec2 ViewportToCanvasSpace(float mouse_x, float mouse_y,
                                       const glm::vec2& display_size,
                                       const glm::vec2& render_res,
                                       entt::registry& registry) {
  float cx = mouse_x;
  float cy = mouse_y;

  if (display_size.x > 0 && display_size.y > 0 && render_res.x > 0 &&
      render_res.y > 0) {
    cx = mouse_x * (render_res.x / display_size.x);
    cy = mouse_y * (render_res.y / display_size.y);
  }

  for (auto e : registry.view<CanvasComponent, CanvasScalerComponent>()) {
    auto& scaler = registry.get<CanvasScalerComponent>(e);
    if (scaler.scale_mode == ScaleMode::ScaleWithScreenSize &&
        render_res.x > 0 && render_res.y > 0) {
      float scale_w = render_res.x / scaler.reference_resolution.x;
      float scale_h = render_res.y / scaler.reference_resolution.y;
      float t = scaler.match_width_or_height;
      float scale_factor = scale_w * (1.0f - t) + scale_h * t;
      cx /= scale_factor;
      cy /= scale_factor;
      break;
    }
  }

  return {cx, cy};
}

struct CameraRay {
  glm::vec3 origin;
  glm::vec3 direction;
  bool valid = false;
};

static CameraRay BuildCameraRay(
    float mouse_x, float mouse_y, const glm::vec2& display_size,
    const std::shared_ptr<CameraData>& camera_data) {
  CameraRay ray;
  if (!camera_data || display_size.x <= 0 || display_size.y <= 0) {
    return ray;
  }

  float ndc_x = (mouse_x / display_size.x) * 2.0f - 1.0f;
  float ndc_y = (mouse_y / display_size.y) * 2.0f - 1.0f;

  glm::mat4 inv_proj = glm::inverse(camera_data->projection);
  glm::mat4 inv_view = glm::inverse(camera_data->view_matrix);

  glm::vec4 near_ndc = {ndc_x, ndc_y, -1.0f, 1.0f};
  glm::vec4 far_ndc = {ndc_x, ndc_y, 1.0f, 1.0f};

  glm::vec4 near_view = inv_proj * near_ndc;
  near_view /= near_view.w;
  glm::vec4 far_view = inv_proj * far_ndc;
  far_view /= far_view.w;

  ray.origin = glm::vec3(inv_view * near_view);
  ray.direction = glm::normalize(glm::vec3(inv_view * far_view) - ray.origin);
  ray.valid = true;
  return ray;
}

static entt::entity FindCanvasRoot(entt::registry& registry,
                                   entt::entity entity) {
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
}

// Returns the player_index for the canvas that owns this entity.
static int GetCanvasPlayerIndex(entt::registry& registry, entt::entity entity) {
  entt::entity canvas = FindCanvasRoot(registry, entity);
  if (canvas != entt::null) {
    return registry.get<CanvasComponent>(canvas).player_index;
  }
  return 0;
}

struct HitCandidate {
  entt::entity entity;
  int32_t draw_order;
};

// Hit test against all interactables owned by canvases with the given
// player_index. Pass player_index = -1 to match all canvases.
static entt::entity HitTest(Scene& scene, float mouse_x, float mouse_y,
                            int player_index) {
  auto& registry = scene.GetRegistry();
  glm::vec2 display_size = scene.GetViewportDisplaySize();
  glm::vec2 render_res = scene.GetRenderResolution();
  if (render_res.x <= 0 || render_res.y <= 0) {
    render_res = display_size;
  }

  glm::vec2 canvas_mouse = ViewportToCanvasSpace(mouse_x, mouse_y, display_size,
                                                 render_res, registry);
  CameraRay ray = BuildCameraRay(mouse_x, mouse_y, display_size,
                                 Engine::renderer()->GetCameraData());

  std::vector<HitCandidate> candidates;

  for (auto entity :
       registry.view<InteractableComponent, RectangleTransformComponent>()) {
    auto& interactable = registry.get<InteractableComponent>(entity);
    if (!interactable.enabled) {
      continue;
    }

    entt::entity canvas_root = FindCanvasRoot(registry, entity);
    if (canvas_root == entt::null) {
      continue;
    }

    auto& canvas = registry.get<CanvasComponent>(canvas_root);

    // Filter by player
    if (player_index >= 0 && canvas.player_index != player_index) {
      continue;
    }

    auto& rt = registry.get<RectangleTransformComponent>(entity);

    if (canvas.render_mode == CanvasRenderMode::WorldSpace) {
      if (!ray.valid || !registry.any_of<TransformComponent>(canvas_root)) {
        continue;
      }

      auto& canvas_transform = registry.get<TransformComponent>(canvas_root);
      glm::mat4 model = canvas_transform.GetTransformMatrix();
      glm::vec3 plane_normal = glm::normalize(glm::vec3(model[2]));
      glm::vec3 plane_origin = glm::vec3(model[3]);

      float denom = glm::dot(plane_normal, ray.direction);
      if (std::abs(denom) < 1e-6f) {
        continue;
      }
      float t = glm::dot(plane_origin - ray.origin, plane_normal) / denom;
      if (t < 0) {
        continue;
      }

      glm::vec3 hit_world = ray.origin + ray.direction * t;
      glm::mat4 inv_model = glm::inverse(model);
      glm::vec3 hit_local = glm::vec3(inv_model * glm::vec4(hit_world, 1.0f));

      glm::vec2 ref_size = {1920, 1080};
      float ppu = 100.0f;
      if (registry.any_of<CanvasScalerComponent>(canvas_root)) {
        auto& scaler = registry.get<CanvasScalerComponent>(canvas_root);
        ref_size = scaler.reference_resolution;
        ppu = std::max(1.0f, scaler.reference_pixels_per_unit);
      }
      glm::vec2 ws = ref_size / ppu;

      glm::vec2 normalized = glm::vec2(hit_local.x, hit_local.y) / ws;
      normalized.y = -normalized.y;
      glm::vec2 canvas_pos = (normalized + glm::vec2(0.5f)) * ref_size;

      if (PointInRect(canvas_pos.x, canvas_pos.y, rt.computed_position,
                      rt.computed_size)) {
        candidates.push_back({entity, rt.draw_order});
      }
    } else {
      if (PointInRect(canvas_mouse.x, canvas_mouse.y, rt.computed_position,
                      rt.computed_size)) {
        candidates.push_back({entity, rt.draw_order});
      }
    }
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const HitCandidate& a, const HitCandidate& b) {
              return a.draw_order > b.draw_order;
            });

  if (!candidates.empty()) {
    return candidates[0].entity;
  }
  return entt::null;
}

// ---------------------------------------------------------------------------
// DispatchWithBubble
// ---------------------------------------------------------------------------
template <typename Fn>
entt::entity UIEventSystem::DispatchWithBubble(entt::registry& registry,
                                               entt::entity start, Fn&& fn) {
  entt::entity walk = start;
  while (walk != entt::null) {
    if (registry.any_of<BehaviorsComponent>(walk)) {
      auto& bc = registry.get<BehaviorsComponent>(walk);
      for (auto& [name, behavior] : bc.behaviors_) {
        if (fn(behavior)) {
          return walk;
        }
      }
    }
    if (registry.any_of<TreeComponent>(walk)) {
      entt::entity parent = registry.get<TreeComponent>(walk).parent;
      if (parent != entt::null &&
          registry.any_of<InteractableComponent>(parent)) {
        walk = parent;
        continue;
      }
    }
    break;
  }
  return entt::null;
}

// ---------------------------------------------------------------------------
// Focus helpers
// ---------------------------------------------------------------------------
void UIEventSystem::FocusEntity(entt::registry& registry, entt::entity entity) {
  if (entity == focused_entity_) {
    return;
  }
  UnfocusEntity(registry);
  if (entity != entt::null && registry.any_of<TextInputComponent>(entity)) {
    auto& input = registry.get<TextInputComponent>(entity);
    input.focused_ = true;
    input.cursor_pos_ = static_cast<int>(input.text.size());
    input.cursor_visible_ = true;
    input.cursor_timer_ = 0.0f;
    focused_entity_ = entity;
  }
}

void UIEventSystem::UnfocusEntity(entt::registry& registry) {
  if (focused_entity_ != entt::null && registry.valid(focused_entity_) &&
      registry.any_of<TextInputComponent>(focused_entity_)) {
    registry.get<TextInputComponent>(focused_entity_).focused_ = false;
  }
  focused_entity_ = entt::null;
}

// ---------------------------------------------------------------------------
// Public accessors
// ---------------------------------------------------------------------------
entt::entity UIEventSystem::GetSelectedEntity(int player_index) const {
  auto it = player_nav_.find(player_index);
  if (it != player_nav_.end()) {
    return it->second.selected_entity;
  }
  return entt::null;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void UIEventSystem::Update(Scene& scene, float delta_time) {
  auto& registry = scene.GetRegistry();

  // Detect if mouse moved (skip hit testing if it hasn't)
  float mx = static_cast<float>(InputManager::GetMouseX()) -
             scene.GetViewportOrigin().x;
  float my = static_cast<float>(InputManager::GetMouseY()) -
             scene.GetViewportOrigin().y;
  if (mx != last_mouse_x_ || my != last_mouse_y_) {
    mouse_dirty_ = true;
    last_mouse_x_ = mx;
    last_mouse_y_ = my;
  }

  // Mouse button changes always need processing
  bool mouse_down =
      InputManager::IsMouseButtonDown(MouseCode::kMouseButtonLeft);
  bool mouse_up = InputManager::IsMouseButtonUp(MouseCode::kMouseButtonLeft);
  if (mouse_down || mouse_up) {
    mouse_dirty_ = true;
  }

  if (mouse_dirty_) {
    ProcessMouseInput(scene);
    mouse_dirty_ = false;
  }

  // Process gamepad input per canvas
  for (auto entity : registry.view<CanvasComponent>()) {
    auto& canvas = registry.get<CanvasComponent>(entity);
    int player = canvas.player_index;

    if (InputManager::GetInputMode(player) == kInputModeGamepad) {
      ProcessGamepadInput(scene, delta_time, player, entity);
    } else {
      // Player switched to keyboard/mouse - clear their selection
      auto it = player_nav_.find(player);
      if (it != player_nav_.end() && it->second.selected_entity != entt::null) {
        ClearSelection(scene, player);
      }
    }
  }

  UpdateButtonStates(registry);
}

// ---------------------------------------------------------------------------
// Mouse input processing
// ---------------------------------------------------------------------------
void UIEventSystem::ProcessMouseInput(Scene& scene) {
  auto& registry = scene.GetRegistry();

  float mouse_x = last_mouse_x_;
  float mouse_y = last_mouse_y_;
  bool mouse_down =
      InputManager::IsMouseButtonDown(MouseCode::kMouseButtonLeft);
  bool mouse_up = InputManager::IsMouseButtonUp(MouseCode::kMouseButtonLeft);

  // Only hit test against canvases owned by keyboard/mouse players
  // Find which players are in mouse mode and hit test for them
  entt::entity hit_entity = entt::null;
  for (auto entity : registry.view<CanvasComponent>()) {
    auto& canvas = registry.get<CanvasComponent>(entity);
    if (InputManager::GetInputMode(canvas.player_index) ==
        kInputModeKeyboardAndMouse) {
      entt::entity hit = HitTest(scene, mouse_x, mouse_y, canvas.player_index);
      if (hit != entt::null) {
        // Pick frontmost across all mouse-mode canvases
        if (hit_entity == entt::null) {
          hit_entity = hit;
        } else {
          auto& rt_hit = registry.get<RectangleTransformComponent>(hit);
          auto& rt_cur = registry.get<RectangleTransformComponent>(hit_entity);
          if (rt_hit.draw_order > rt_cur.draw_order) {
            hit_entity = hit;
          }
        }
      }
    }
  }

  // Hover enter/exit
  if (hit_entity != hovered_entity_) {
    if (hovered_entity_ != entt::null && registry.valid(hovered_entity_)) {
      if (registry.any_of<InteractableComponent>(hovered_entity_)) {
        registry.get<InteractableComponent>(hovered_entity_).hovered_ = false;
      }
      DispatchWithBubble(registry, hovered_entity_, [](IBehavior* b) {
        b->OnPointerExit();
        return false;
      });
    }

    if (hit_entity != entt::null) {
      registry.get<InteractableComponent>(hit_entity).hovered_ = true;
      DispatchWithBubble(registry, hit_entity, [](IBehavior* b) {
        b->OnPointerEnter();
        return false;
      });
    }

    hovered_entity_ = hit_entity;
  }

  // Pointer down
  if (mouse_down) {
    // Text input focus
    entt::entity new_focus = entt::null;
    if (hit_entity != entt::null &&
        registry.any_of<TextInputComponent>(hit_entity)) {
      new_focus = hit_entity;
    }
    if (new_focus != focused_entity_) {
      if (new_focus != entt::null) {
        FocusEntity(registry, new_focus);
      } else {
        UnfocusEntity(registry);
      }
    }

    if (hit_entity != entt::null) {
      registry.get<InteractableComponent>(hit_entity).pressed_ = true;
      pressed_entity_ = hit_entity;

      DispatchWithBubble(registry, hit_entity,
                         [mouse_x, mouse_y](IBehavior* b) {
                           return b->OnPointerDown(mouse_x, mouse_y);
                         });
    }
  }

  // Pointer up
  if (mouse_up) {
    if (pressed_entity_ != entt::null && registry.valid(pressed_entity_)) {
      if (registry.any_of<InteractableComponent>(pressed_entity_)) {
        registry.get<InteractableComponent>(pressed_entity_).pressed_ = false;
      }

      if (pressed_entity_ == hit_entity) {
        DispatchWithBubble(registry, pressed_entity_,
                           [mouse_x, mouse_y](IBehavior* b) {
                             return b->OnPointerClick(mouse_x, mouse_y);
                           });
      }

      DispatchWithBubble(registry, pressed_entity_,
                         [mouse_x, mouse_y](IBehavior* b) {
                           return b->OnPointerUp(mouse_x, mouse_y);
                         });
    }
    pressed_entity_ = entt::null;
  }

  // Forward mouse events to UIDocument contexts
  // Use canvas-space coordinates (same space as computed_position/computed_size)
  glm::vec2 display_size = scene.GetViewportDisplaySize();
  glm::vec2 render_res = scene.GetRenderResolution();
  if (render_res.x <= 0 || render_res.y <= 0) {
    render_res = display_size;
  }
  glm::vec2 canvas_mouse = ViewportToCanvasSpace(mouse_x, mouse_y, display_size,
                                                 render_res, registry);

  for (auto entity :
       registry.view<UIDocumentComponent, RectangleTransformComponent>()) {
    auto& doc = registry.get<UIDocumentComponent>(entity);
    if (!doc.rml_context_ || !doc.visible) {
      continue;
    }
    auto& rt = registry.get<RectangleTransformComponent>(entity);

    // Canvas-space to document-local coords
    float local_x = canvas_mouse.x - rt.computed_position.x;
    float local_y = canvas_mouse.y - rt.computed_position.y;

    bool inside = local_x >= 0 && local_y >= 0 &&
                  local_x < rt.computed_size.x && local_y < rt.computed_size.y;

    // Scale from canvas-space to offscreen render resolution
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    if (doc.offscreen_size_.x > 0 && rt.computed_size.x > 0) {
      scale_x = doc.offscreen_size_.x / rt.computed_size.x;
      scale_y = doc.offscreen_size_.y / rt.computed_size.y;
    }

    int rml_x = static_cast<int>(local_x * scale_x);
    int rml_y = static_cast<int>(local_y * scale_y);

    doc.rml_context_->ProcessMouseMove(rml_x, rml_y, 0);

    if (inside) {
      if (mouse_down) {
        doc.rml_context_->ProcessMouseButtonDown(0, 0);
      }
      if (mouse_up) {
        doc.rml_context_->ProcessMouseButtonUp(0, 0);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Gamepad input processing (per-player, scoped to a canvas)
// ---------------------------------------------------------------------------
void UIEventSystem::ProcessGamepadInput(Scene& scene, float delta_time,
                                        int player_index,
                                        entt::entity canvas_entity) {
  auto& registry = scene.GetRegistry();
  auto& nav = player_nav_[player_index];

  // Validate current selection still exists and belongs to this canvas
  if (nav.selected_entity != entt::null) {
    if (!registry.valid(nav.selected_entity) ||
        FindCanvasRoot(registry, nav.selected_entity) != canvas_entity) {
      nav.selected_entity = entt::null;
    }
  }

  // Select first navigable element if nothing is selected
  if (nav.selected_entity == entt::null) {
    entt::entity first = FindFirstNavigable(scene, canvas_entity);
    if (first != entt::null) {
      NavigateTo(scene, player_index, first);
    }
    return;
  }

  // Get the gamepad assigned to this player
  const auto& slot = InputManager::GetPlayerSlot(player_index);
  int gp = slot.gamepad_index;
  if (gp < 0 || !InputManager::GetGamepadState(gp).connected) {
    return;
  }

  const auto& state = InputManager::GetGamepadState(gp);

  // Navigation direction from d-pad or left stick
  glm::vec2 nav_dir = {0.0f, 0.0f};

  bool dpad_up = state.buttons[static_cast<int>(GamepadButtonDPadUp)].pressed;
  bool dpad_down =
      state.buttons[static_cast<int>(GamepadButtonDPadDown)].pressed;
  bool dpad_left =
      state.buttons[static_cast<int>(GamepadButtonDPadLeft)].pressed;
  bool dpad_right =
      state.buttons[static_cast<int>(GamepadButtonDPadRight)].pressed;

  if (dpad_up) {
    nav_dir.y = -1.0f;
  }
  if (dpad_down) {
    nav_dir.y = 1.0f;
  }
  if (dpad_left) {
    nav_dir.x = -1.0f;
  }
  if (dpad_right) {
    nav_dir.x = 1.0f;
  }

  constexpr float kNavDeadzone = 0.5f;
  if (nav_dir.x == 0.0f && nav_dir.y == 0.0f) {
    float lx = state.axes[static_cast<int>(GamepadAxisLeftX)];
    float ly = state.axes[static_cast<int>(GamepadAxisLeftY)];
    if (std::abs(lx) > kNavDeadzone || std::abs(ly) > kNavDeadzone) {
      if (std::abs(lx) > std::abs(ly)) {
        nav_dir.x = lx > 0 ? 1.0f : -1.0f;
      } else {
        nav_dir.y = ly > 0 ? 1.0f : -1.0f;
      }
    }
  }

  // Navigation with repeat
  bool nav_active = (nav_dir.x != 0.0f || nav_dir.y != 0.0f);
  if (nav_active) {
    bool should_navigate = false;
    if (!nav.nav_held) {
      should_navigate = true;
      nav.nav_held = true;
      nav.nav_repeat_timer = 0.0f;
    } else {
      nav.nav_repeat_timer += delta_time;
      if (nav.nav_repeat_timer >= kNavRepeatDelay) {
        float repeat_elapsed = nav.nav_repeat_timer - kNavRepeatDelay;
        float prev = repeat_elapsed - delta_time;
        if (prev < 0.0f || static_cast<int>(repeat_elapsed / kNavRepeatRate) >
                               static_cast<int>(prev / kNavRepeatRate)) {
          should_navigate = true;
        }
      }
    }

    if (should_navigate) {
      entt::entity neighbor =
          FindNeighbor(scene, nav.selected_entity, nav_dir, canvas_entity);
      if (neighbor != entt::null) {
        NavigateTo(scene, player_index, neighbor);
      }
    }
  } else {
    nav.nav_held = false;
    nav.nav_repeat_timer = 0.0f;
  }

  // Submit (A button)
  const auto& a_btn = state.buttons[static_cast<int>(GamepadButtonA)];
  if (a_btn.pressed && !a_btn.previous_pressed) {
    if (registry.any_of<InteractableComponent>(nav.selected_entity)) {
      registry.get<InteractableComponent>(nav.selected_entity).pressed_ = true;
    }
    DispatchWithBubble(registry, nav.selected_entity,
                       [](IBehavior* b) { return b->OnPointerDown(0, 0); });
  }
  if (!a_btn.pressed && a_btn.previous_pressed) {
    if (registry.any_of<InteractableComponent>(nav.selected_entity)) {
      registry.get<InteractableComponent>(nav.selected_entity).pressed_ = false;
    }
    DispatchWithBubble(registry, nav.selected_entity,
                       [](IBehavior* b) { return b->OnSubmit(); });
    DispatchWithBubble(registry, nav.selected_entity,
                       [](IBehavior* b) { return b->OnPointerClick(0, 0); });
    DispatchWithBubble(registry, nav.selected_entity,
                       [](IBehavior* b) { return b->OnPointerUp(0, 0); });
  }

  // Cancel (B button)
  const auto& b_btn = state.buttons[static_cast<int>(GamepadButtonB)];
  if (b_btn.pressed && !b_btn.previous_pressed) {
    DispatchWithBubble(registry, nav.selected_entity,
                       [](IBehavior* b) { return b->OnCancel(); });
  }
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
void UIEventSystem::ClearSelection(Scene& scene, int player_index) {
  auto& registry = scene.GetRegistry();
  auto it = player_nav_.find(player_index);
  if (it == player_nav_.end()) {
    return;
  }

  entt::entity selected = it->second.selected_entity;
  if (selected != entt::null && registry.valid(selected)) {
    if (registry.any_of<InteractableComponent>(selected)) {
      registry.get<InteractableComponent>(selected).selected_ = false;
    }
    DispatchWithBubble(registry, selected, [](IBehavior* b) {
      b->OnDeselect();
      return false;
    });
  }
  it->second.selected_entity = entt::null;
}

void UIEventSystem::NavigateTo(Scene& scene, int player_index,
                               entt::entity target) {
  auto& registry = scene.GetRegistry();
  auto& nav = player_nav_[player_index];

  if (target == nav.selected_entity) {
    return;
  }

  // Deselect old
  if (nav.selected_entity != entt::null &&
      registry.valid(nav.selected_entity)) {
    if (registry.any_of<InteractableComponent>(nav.selected_entity)) {
      registry.get<InteractableComponent>(nav.selected_entity).selected_ =
          false;
    }
    DispatchWithBubble(registry, nav.selected_entity, [](IBehavior* b) {
      b->OnDeselect();
      return false;
    });
  }

  // Select new
  nav.selected_entity = target;
  if (target != entt::null) {
    if (registry.any_of<InteractableComponent>(target)) {
      registry.get<InteractableComponent>(target).selected_ = true;
    }
    DispatchWithBubble(registry, target, [](IBehavior* b) {
      b->OnSelect();
      return false;
    });

    // Auto-focus text inputs
    if (registry.any_of<TextInputComponent>(target)) {
      FocusEntity(registry, target);
    }
  }
}

entt::entity UIEventSystem::FindNeighbor(Scene& scene, entt::entity from,
                                         glm::vec2 direction,
                                         entt::entity canvas_root) {
  auto& registry = scene.GetRegistry();

  if (!registry.any_of<NavigableComponent>(from)) {
    return entt::null;
  }

  // Check explicit overrides first
  auto& nav_comp = registry.get<NavigableComponent>(from);
  entt::entity explicit_target = entt::null;
  if (direction.x > 0.5f) {
    explicit_target = nav_comp.nav_right;
  } else if (direction.x < -0.5f) {
    explicit_target = nav_comp.nav_left;
  } else if (direction.y > 0.5f) {
    explicit_target = nav_comp.nav_down;
  } else if (direction.y < -0.5f) {
    explicit_target = nav_comp.nav_up;
  }

  if (explicit_target != entt::null && registry.valid(explicit_target) &&
      registry.any_of<InteractableComponent>(explicit_target) &&
      registry.get<InteractableComponent>(explicit_target).enabled) {
    return explicit_target;
  }

  // Auto-navigation: find best candidate in the given direction
  if (!registry.any_of<RectangleTransformComponent>(from)) {
    return entt::null;
  }

  auto& from_rt = registry.get<RectangleTransformComponent>(from);
  glm::vec2 from_center =
      from_rt.computed_position + from_rt.computed_size * 0.5f;

  glm::vec2 norm_dir = glm::normalize(direction);

  entt::entity best = entt::null;
  float best_score = -1.0f;

  auto view = registry.view<NavigableComponent, InteractableComponent,
                            RectangleTransformComponent>();
  for (auto entity : view) {
    if (entity == from) {
      continue;
    }

    auto& interactable = registry.get<InteractableComponent>(entity);
    if (!interactable.enabled) {
      continue;
    }

    // Must be in the same canvas
    if (FindCanvasRoot(registry, entity) != canvas_root) {
      continue;
    }

    auto& rt = registry.get<RectangleTransformComponent>(entity);
    glm::vec2 center = rt.computed_position + rt.computed_size * 0.5f;
    glm::vec2 offset = center - from_center;

    float dist = glm::length(offset);
    if (dist < 0.001f) {
      continue;
    }

    glm::vec2 norm_offset = offset / dist;
    float dot = glm::dot(norm_offset, norm_dir);

    // Must be in the general direction
    if (dot < 0.3f) {
      continue;
    }

    // Score: favor alignment and proximity
    float score = dot / (1.0f + dist * 0.01f);
    if (score > best_score) {
      best_score = score;
      best = entity;
    }
  }

  // Wrap-around: if no candidate found, find the farthest element in the
  // opposite direction (wraps to the other end)
  if (best == entt::null) {
    float farthest_dist = -1.0f;
    for (auto entity : view) {
      if (entity == from) {
        continue;
      }

      auto& interactable = registry.get<InteractableComponent>(entity);
      if (!interactable.enabled) {
        continue;
      }

      if (FindCanvasRoot(registry, entity) != canvas_root) {
        continue;
      }

      auto& rt = registry.get<RectangleTransformComponent>(entity);
      glm::vec2 center = rt.computed_position + rt.computed_size * 0.5f;
      glm::vec2 offset = center - from_center;

      float dot = glm::dot(glm::normalize(offset), norm_dir);
      // Element is in the opposite direction
      if (dot < -0.3f) {
        float dist = glm::length(offset);
        if (dist > farthest_dist) {
          farthest_dist = dist;
          best = entity;
        }
      }
    }
  }

  return best;
}

entt::entity UIEventSystem::FindFirstNavigable(Scene& scene,
                                               entt::entity canvas_root) {
  auto& registry = scene.GetRegistry();
  auto view = registry.view<NavigableComponent, InteractableComponent,
                            RectangleTransformComponent>();

  entt::entity best = entt::null;
  int32_t best_order = -1;

  for (auto entity : view) {
    auto& interactable = registry.get<InteractableComponent>(entity);
    if (!interactable.enabled) {
      continue;
    }

    // Must belong to the specified canvas
    if (FindCanvasRoot(registry, entity) != canvas_root) {
      continue;
    }

    auto& rt = registry.get<RectangleTransformComponent>(entity);
    if (best == entt::null || rt.draw_order > best_order) {
      best = entity;
      best_order = rt.draw_order;
    }
  }

  return best;
}

// ---------------------------------------------------------------------------
// Button state update
// ---------------------------------------------------------------------------
void UIEventSystem::UpdateButtonStates(entt::registry& registry) {
  for (auto entity : registry.view<ButtonComponent, InteractableComponent>()) {
    auto& btn = registry.get<ButtonComponent>(entity);
    auto& interactable = registry.get<InteractableComponent>(entity);

    if (!interactable.enabled) {
      btn.state_ = ButtonState::Disabled;
    } else if (interactable.pressed_) {
      btn.state_ = ButtonState::Pressed;
    } else if (interactable.hovered_) {
      btn.state_ = ButtonState::Hovered;
    } else if (interactable.selected_) {
      btn.state_ = ButtonState::Selected;
    } else {
      btn.state_ = ButtonState::Normal;
    }
  }
}

}  // namespace Wiesel