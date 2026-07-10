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

#include "asset/w_asset_handle.h"
#include "core/w_reflect.h"
#include "rendering/w_descriptor.h"
#include "rendering/w_texture.h"
#include "scene/w_components.h"

namespace wiesel {

enum class BillboardOcclusionMode : uint8_t {
  Disabled = 0,       // Hidden when geometry is in front
  Faded = 1,          // Rendered with reduced alpha when occluded
  AlwaysVisible = 2,  // Always rendered on top
};

// World-space textured quad that always faces the camera. Size is in screen
// pixels at a reference distance, clamped to min_size / max_size for distance
// fade. Multiple billboards can be stacked on an entity hierarchy using
// sort_layer (parent = background, child = foreground bar, etc.).
// Nine-slice is taken from the texture's asset properties when present.
WCLASS()

struct BillboardRendererComponent : public IComponent {
  WPROPERTY(Serializable)
  AssetHandle texture_handle;

  // Screen-space size in pixels at 1-meter reference distance.
  // Billboard shrinks naturally with distance (perspective) from this size.
  WPROPERTY(Serializable, Animatable)
  glm::vec2 size = {64.0f, 64.0f};

  // Scalar multipliers that clamp the distance-based scaling.
  // min_size = 0.5 means the billboard never gets smaller than 50% of `size`.
  // max_size = 2.0 means it never exceeds 200% of `size`.
  // Set both to 1.0 to lock the screen-space size (constant pixels).
  WPROPERTY(Serializable)
  float min_size = 0.25f;
  WPROPERTY(Serializable)
  float max_size = 4.0f;

  // Pivot: 0 = left/top, 0.5 = center, 1 = right/bottom.
  WPROPERTY(Serializable)
  glm::vec2 pivot = {0.5f, 0.5f};

  WPROPERTY(Serializable, Animatable)
  glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};

  // Higher values draw on top.
  WPROPERTY(Serializable)
  int32_t sort_layer = 0;

  // Depth behavior when geometry is in front.
  WPROPERTY(Serializable)
  BillboardOcclusionMode occlusion = BillboardOcclusionMode::Disabled;

  // Alpha multiplier applied to occluded parts when occlusion == Faded.
  WPROPERTY(Serializable)
  float occluded_alpha = 0.3f;

  // Cached runtime resources (not serialized)
  std::shared_ptr<Texture> cached_texture;
  std::shared_ptr<DescriptorSet> cached_descriptor;
  AssetHandle bound_handle;
};

}  // namespace wiesel
