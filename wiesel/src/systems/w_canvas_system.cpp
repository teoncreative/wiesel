
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_canvas_system.hpp"
#include <algorithm>
#include "scene/w_entity.hpp"
#include "ui/w_font.hpp"

namespace Wiesel {

void CanvasSystem::Update(Scene& scene, glm::vec2 screen_size) {
  PROFILE_ZONE_SCOPED_N("CanvasSystem::Update");
  // Collect canvas roots and sort by sort_order so higher values draw on top
  std::vector<entt::entity> canvas_roots;
  for (const auto& handle : scene.GetAllEntitiesWith<CanvasComponent>()) {
    canvas_roots.push_back(handle);
  }
  std::sort(canvas_roots.begin(), canvas_roots.end(),
            [&scene](entt::entity a, entt::entity b) {
              return scene.GetComponent<CanvasComponent>(a).sort_order
                   < scene.GetComponent<CanvasComponent>(b).sort_order;
            });

  int32_t draw_order = 0;
  for (const auto& handle : canvas_roots) {
    Entity entity{handle, &scene};
    auto& canvas = entity.GetComponent<CanvasComponent>();

    // Compute effective screen size based on canvas scaler
    glm::vec2 effective_size = screen_size;
    if (entity.HasComponent<CanvasScalerComponent>()) {
      auto& scaler = entity.GetComponent<CanvasScalerComponent>();
      if (scaler.scale_mode == ScaleMode::ScaleWithScreenSize) {
        // Compute scale factor by blending width and height ratios
        float scale_w = screen_size.x / scaler.reference_resolution.x;
        float scale_h = screen_size.y / scaler.reference_resolution.y;
        float t = scaler.match_width_or_height;
        float scale_factor = scale_w * (1.0f - t) + scale_h * t;
        // Express screen in reference-resolution units
        effective_size = screen_size / scale_factor;
      }
    }

    // Canvas root rect
    glm::vec2 parent_pos = {0, 0};
    glm::vec2 parent_size = effective_size;

    // If the canvas root itself has a RectangleTransformComponent, apply it
    if (entity.HasComponent<RectangleTransformComponent>()) {
      auto& rt = entity.GetComponent<RectangleTransformComponent>();
      glm::vec2 anchor_origin =
          ComputeAnchorOrigin(rt.anchor, effective_size);
      glm::vec2 resolved_size;
      resolved_size.x = rt.size_mode_x == SizeMode::Percent
                            ? rt.size.x * effective_size.x
                            : rt.size.x;
      resolved_size.y = rt.size_mode_y == SizeMode::Percent
                            ? rt.size.y * effective_size.y
                            : rt.size.y;
      rt.computed_size = resolved_size * rt.scale;
      glm::vec2 pivot_offset =
          ComputeAnchorOrigin(rt.pivot, rt.computed_size);
      rt.computed_position = anchor_origin - pivot_offset + rt.position;
      rt.draw_order = draw_order++;
      parent_pos = rt.computed_position;
      parent_size = rt.computed_size;
    }

    LayoutChildren(scene, handle, parent_pos, parent_size, &canvas,
                   draw_order);
  }
}

glm::vec2 CanvasSystem::ComputeAnchorOrigin(AnchorPreset anchor,
                                             glm::vec2 parent_size) {
  switch (anchor) {
    case AnchorPreset::TopLeft:
      return {0, 0};
    case AnchorPreset::TopCenter:
      return {parent_size.x * 0.5f, 0};
    case AnchorPreset::TopRight:
      return {parent_size.x, 0};
    case AnchorPreset::MiddleLeft:
      return {0, parent_size.y * 0.5f};
    case AnchorPreset::MiddleCenter:
      return {parent_size.x * 0.5f, parent_size.y * 0.5f};
    case AnchorPreset::MiddleRight:
      return {parent_size.x, parent_size.y * 0.5f};
    case AnchorPreset::BottomLeft:
      return {0, parent_size.y};
    case AnchorPreset::BottomCenter:
      return {parent_size.x * 0.5f, parent_size.y};
    case AnchorPreset::BottomRight:
      return {parent_size.x, parent_size.y};
    case AnchorPreset::StretchAll:
      return {0, 0};
    default:
      return {0, 0};
  }
}

void CanvasSystem::LayoutChildren(Scene& scene, entt::entity parent,
                                  glm::vec2 parent_pos,
                                  glm::vec2 parent_size,
                                  const CanvasComponent* canvas,
                                  int32_t& draw_order) {
  if (!scene.HasComponent<TreeComponent>(parent)) {
    return;
  }
  auto& tree = scene.GetComponent<TreeComponent>(parent);

  // Apply parent padding to get content area
  glm::vec2 content_pos = parent_pos;
  glm::vec2 content_size = parent_size;
  if (scene.HasComponent<RectangleTransformComponent>(parent)) {
    auto& parent_rt =
        scene.GetComponent<RectangleTransformComponent>(parent);
    content_pos.x += parent_rt.padding.x;   // left
    content_pos.y += parent_rt.padding.y;   // top
    content_size.x -= parent_rt.padding.x + parent_rt.padding.z;  // left+right
    content_size.y -= parent_rt.padding.y + parent_rt.padding.w;  // top+bottom
  }

  // Initialize cursor with start spacing
  float cursor = (canvas ? canvas->start_spacing : 0.0f);

  for (entt::entity child : tree.childs) {
    if (!scene.HasComponent<RectangleTransformComponent>(child)) {
      continue;
    }
    auto& rt = scene.GetComponent<RectangleTransformComponent>(child);

    // Resolve size - auto-size from text only when size is zero (auto)
    glm::vec2 resolved_size;
    resolved_size.x = rt.size_mode_x == SizeMode::Percent
                          ? rt.size.x * content_size.x
                          : rt.size.x;
    resolved_size.y = rt.size_mode_y == SizeMode::Percent
                          ? rt.size.y * content_size.y
                          : rt.size.y;

    if (scene.HasComponent<TextComponent>(child) &&
        resolved_size.x == 0 && resolved_size.y == 0) {
      auto& text = scene.GetComponent<TextComponent>(child);
      if (!text.text.empty()) {
        std::shared_ptr<Font> font = FontCache::Get(text.font_handle, text.font_size);
        if (font && font->IsLoaded()) {
          resolved_size = font->MeasureText(text.text, text.font_size);
        }
      }
    }
    rt.computed_size = resolved_size * rt.scale;

    if (!canvas || canvas->direction == LayoutDirection::None) {
      // Free positioning: anchor on parent - pivot on self + offset
      glm::vec2 anchor_origin =
          ComputeAnchorOrigin(rt.anchor, content_size);
      glm::vec2 pivot_offset =
          ComputeAnchorOrigin(rt.pivot, rt.computed_size);
      rt.computed_position =
          content_pos + anchor_origin - pivot_offset + rt.position
          + glm::vec2(rt.margin.x, rt.margin.y);
    } else if (canvas->direction == LayoutDirection::Row) {
      cursor += rt.margin.x;  // left margin
      rt.computed_position.x = content_pos.x + cursor;
      switch (canvas->alignment) {
        case ChildAlignment::Start:
          rt.computed_position.y = content_pos.y + rt.margin.y;
          break;
        case ChildAlignment::Center:
          rt.computed_position.y =
              content_pos.y + (content_size.y - rt.computed_size.y) * 0.5f;
          break;
        case ChildAlignment::End:
          rt.computed_position.y =
              content_pos.y + content_size.y - rt.computed_size.y - rt.margin.w;
          break;
      }
      cursor += rt.computed_size.x + rt.margin.z + canvas->spacing;
    } else if (canvas->direction == LayoutDirection::Column) {
      cursor += rt.margin.y;  // top margin
      rt.computed_position.y = content_pos.y + cursor;
      switch (canvas->alignment) {
        case ChildAlignment::Start:
          rt.computed_position.x = content_pos.x + rt.margin.x;
          break;
        case ChildAlignment::Center:
          rt.computed_position.x =
              content_pos.x + (content_size.x - rt.computed_size.x) * 0.5f;
          break;
        case ChildAlignment::End:
          rt.computed_position.x =
              content_pos.x + content_size.x - rt.computed_size.x - rt.margin.z;
          break;
      }
      cursor += rt.computed_size.y + rt.margin.w + canvas->spacing;
    }

    rt.draw_order = draw_order++;

    // Apply button state offset to children's parent position
    glm::vec2 child_origin = rt.computed_position;
    if (scene.HasComponent<ButtonComponent>(child)) {
      auto& btn = scene.GetComponent<ButtonComponent>(child);
      if (btn.state_ == ButtonState::Pressed) {
        child_origin += btn.pressed_offset;
      } else if (btn.state_ == ButtonState::Hovered) {
        child_origin += btn.hovered_offset;
      }
    }

    // Recurse into children. If child has its own CanvasComponent,
    // use its layout settings; otherwise inherit parent's.
    const CanvasComponent* child_canvas = nullptr;
    if (scene.HasComponent<CanvasComponent>(child)) {
      child_canvas = &scene.GetComponent<CanvasComponent>(child);
    }
    LayoutChildren(scene, child, child_origin, rt.computed_size,
                   child_canvas, draw_order);
  }
}

void CanvasSystem::OnEvent(Wiesel::Event& event) {}
}  // namespace Wiesel
