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

#include "animation/w_animation.h"
#include "asset/w_asset_handle.h"

namespace wiesel {

// How keyframe values are interpolated between keys.
enum class CurveInterp {
  Linear,  // lerp (float, vec2/3/4), slerp (quat)
  Step,    // hold previous value until next key (AssetHandle, bool, int)
};

// A property curve targeting a component field via WPROPERTY reflection.
// At runtime the target is resolved to an entt::meta_data for get/set.
struct PropertyCurve {
  std::string target_component;  // e.g. "SpriteRendererComponent"
  std::string target_field;      // e.g. "sprite_handle"
  CurveInterp interp = CurveInterp::Linear;

  // Only one of these is populated per curve, matching the field's type.
  std::vector<AnimationKey<float>> float_keys;
  std::vector<AnimationKey<glm::vec2>> vec2_keys;
  std::vector<AnimationKey<glm::vec3>> vec3_keys;
  std::vector<AnimationKey<glm::vec4>> vec4_keys;
  std::vector<AnimationKey<glm::quat>> quat_keys;
  std::vector<AnimationKey<int>> int_keys;
  std::vector<AnimationKey<bool>> bool_keys;
  std::vector<AnimationKey<AssetHandle>> asset_keys;
};

// Unified animation clip asset (.wanimclip).
//
// Can contain property curves (for animating any WPROPERTY(Animatable) field)
// and/or bone channels (for skeletal transforms within a Model's node hierarchy).
// A sprite-only clip has property curves. A skeletal clip has bone channels.
// Mixed clips can have both.
struct AnimClipAssetData {
  float duration = 0.0f;
  float ticks_per_second = 25.0f;
  bool loop = true;

  // Animate any reflected component field.
  std::vector<PropertyCurve> property_curves;

  // Skeletal bone transform animation (reuse existing AnimationChannel format).
  // These target the Model's node hierarchy, not ECS components.
  std::vector<AnimationChannel> bone_channels;

  // Maximum distance any bone position keyframe reaches from the origin.
  // Used to expand culling bounds for skeletal animation.
  // Computed at extraction time and serialized in the asset.
  float max_bone_reach = 0.0f;

  bool HasBoneChannels() const { return !bone_channels.empty(); }

  bool HasPropertyCurves() const { return !property_curves.empty(); }
};

}  // namespace wiesel