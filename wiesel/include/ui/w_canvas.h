
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

#include "entt/entity/entity.hpp"
#include "w_pch.h"

namespace Wiesel {

enum class AnchorPreset {
  TopLeft = 0,
  TopCenter = 1,
  TopRight = 2,
  MiddleLeft = 3,
  MiddleCenter = 4,
  MiddleRight = 5,
  BottomLeft = 6,
  BottomCenter = 7,
  BottomRight = 8,
  StretchAll = 9
};

enum class SizeMode { Fixed = 0, Percent = 1 };

enum class LayoutDirection { None = 0, Row = 1, Column = 2 };

enum class ChildAlignment { Start = 0, Center = 1, End = 2 };

enum class CanvasRenderMode {
  ScreenSpaceOverlay = 0,  // 2D overlay on top of everything
  ScreenSpaceCamera = 1,   // flat plane at distance in front of camera
  WorldSpace = 2,          // 3D positioned via TransformComponent
};

enum class ScaleMode {
  ConstantPixelSize = 0,   // 1:1 pixel mapping, no scaling
  ScaleWithScreenSize = 1  // scale relative to a reference resolution
};

struct CanvasComponent {
  CanvasRenderMode render_mode = CanvasRenderMode::ScreenSpaceOverlay;
  LayoutDirection direction = LayoutDirection::None;
  ChildAlignment alignment = ChildAlignment::Start;
  float spacing = 0.0f;
  float start_spacing = 0.0f;  // space before first child
  float end_spacing = 0.0f;    // space after last child
  int sort_order = 0;

  // ScreenSpaceCamera settings
  float plane_distance = 10.0f;
  entt::entity camera_entity =
      entt::null;  // which camera to render in front of

  // Which player's input drives this canvas.
  // Overlay canvases default to player 0.
  int player_index = 0;
};

struct CanvasScalerComponent {
  ScaleMode scale_mode = ScaleMode::ConstantPixelSize;
  glm::vec2 reference_resolution = {1920.0f, 1080.0f};

  // 0 = match width, 1 = match height, 0.5 = blend both (ScreenSpace modes)
  float match_width_or_height = 0.5f;

  // WorldSpace: how many canvas pixels correspond to 1 world unit
  float reference_pixels_per_unit = 100.0f;
};

}  // namespace Wiesel
