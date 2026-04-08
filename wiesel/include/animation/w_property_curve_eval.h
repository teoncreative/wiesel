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

#include "animation/w_animation_clip_asset.h"

#include <entt/entt.hpp>

namespace Wiesel {

class Scene;

// Evaluate all property curves in a clip at the given time and write
// the results to the entity's components via the reflection system.
// Only targets fields marked WPROPERTY(Animatable).
void EvaluatePropertyCurves(Scene& scene, entt::entity entity,
                            const std::vector<PropertyCurve>& curves,
                            float time);

// Blend property curves between two clips (for crossfade transitions).
// Evaluates both sets of curves at their respective times, then
// interpolates the results using blend_weight (0 = fully A, 1 = fully B).
// Step-interpolated curves snap to B when blend_weight >= 0.5.
void BlendPropertyCurves(Scene& scene, entt::entity entity,
                         const std::vector<PropertyCurve>& curves_a,
                         float time_a,
                         const std::vector<PropertyCurve>& curves_b,
                         float time_b, float blend_weight);

}  // namespace Wiesel
