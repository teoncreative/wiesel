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
#include "rendering/w_billboard_renderer.h"
#include "rendering/w_descriptor.h"
#include "scene/w_components.h"

namespace wiesel {

enum class TextAlignment : uint8_t {
  Left = 0,
  Center = 1,
  Right = 2,
};

// World-space text label drawn as a camera-facing billboard.
// Uses the Font/FontCache system; glyphs are sized in pixels at a 1-meter
// reference distance and scale with perspective (clamped by min/max multiplier).
WCLASS()

struct BillboardTextComponent : public IComponent {
  WPROPERTY(Serializable)
  AssetHandle font_handle;

  WPROPERTY(Serializable, Animatable)
  std::string text;

  // Font size in pixels at 1-meter reference distance.
  WPROPERTY(Serializable, Animatable)
  float font_size = 32.0f;

  WPROPERTY(Serializable, Animatable)
  glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

  WPROPERTY(Serializable)
  TextAlignment alignment = TextAlignment::Center;

  // Distance-scaling clamp (same semantics as BillboardRendererComponent).
  WPROPERTY(Serializable)
  float min_size = 0.25f;
  WPROPERTY(Serializable)
  float max_size = 4.0f;

  WPROPERTY(Serializable)
  int32_t sort_layer = 0;

  WPROPERTY(Serializable)
  BillboardOcclusionMode occlusion = BillboardOcclusionMode::Disabled;

  WPROPERTY(Serializable)
  float occluded_alpha = 0.3f;

  // Cached font descriptor (not serialized)
  std::shared_ptr<DescriptorSet> cached_descriptor;
  void* cached_atlas_ptr = nullptr;  // detect atlas changes
};

}  // namespace wiesel
