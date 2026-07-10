//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "animation/w_property_curve_eval.h"

#include "core/w_reflect_facade.h"
#include "scene/w_scene.h"

namespace wiesel {

// Find the last keyframe at or before the given time (step interpolation).
template <typename T>
static const T* SampleStep(const std::vector<AnimationKey<T>>& keys,
                           float time) {
  if (keys.empty()) {
    return nullptr;
  }
  const T* result = &keys[0].value;
  for (const auto& k : keys) {
    if (k.time <= time) {
      result = &k.value;
    } else {
      break;
    }
  }
  return result;
}

// Linearly interpolate between the two surrounding keyframes.
template <typename T>
static T SampleLinear(const std::vector<AnimationKey<T>>& keys, float time) {
  if (keys.empty()) {
    return T{};
  }
  if (keys.size() == 1 || time <= keys.front().time) {
    return keys.front().value;
  }
  if (time >= keys.back().time) {
    return keys.back().value;
  }

  // Find the two surrounding keys
  for (size_t i = 0; i + 1 < keys.size(); i++) {
    if (time >= keys[i].time && time < keys[i + 1].time) {
      float segment = keys[i + 1].time - keys[i].time;
      if (segment <= 0.0f) {
        return keys[i].value;
      }
      float t = (time - keys[i].time) / segment;
      if constexpr (std::is_same_v<T, glm::quat>) {
        return glm::slerp(keys[i].value, keys[i + 1].value, t);
      } else {
        return glm::mix(keys[i].value, keys[i + 1].value, t);
      }
    }
  }
  return keys.back().value;
}

// Resolve the target component + field on an entity. Returns false if
// the component or field is missing or not animatable.
struct ResolvedTarget {
  reflect::TypeHandle type;
  reflect::FieldHandle field;
  void* component_ptr = nullptr;
};

static bool ResolveTarget(Scene& scene, entt::entity entity,
                          const std::string& component_name,
                          const std::string& field_name, ResolvedTarget& out) {
  out.type = reflect::FindType(component_name);
  if (!out.type) {
    return false;
  }

  out.field = reflect::FindField(out.type, field_name);
  if (!out.field) {
    return false;
  }

  if (!out.field.Attrs().animatable) {
    return false;
  }

  out.component_ptr =
      reflect::GetComponentRaw(scene.GetRegistry(), entity, out.type);
  return out.component_ptr != nullptr;
}

void EvaluatePropertyCurves(Scene& scene, entt::entity entity,
                            const std::vector<PropertyCurve>& curves,
                            float time) {
  for (const auto& curve : curves) {
    ResolvedTarget target;
    if (!ResolveTarget(scene, entity, curve.target_component,
                       curve.target_field, target)) {
      continue;
    }

    bool is_step = (curve.interp == CurveInterp::Step);

    if (!curve.float_keys.empty()) {
      float val = is_step ? *SampleStep(curve.float_keys, time)
                          : SampleLinear(curve.float_keys, time);
      reflect::SetFloat(target.field, target.component_ptr, val);
    } else if (!curve.vec2_keys.empty()) {
      glm::vec2 val = is_step ? *SampleStep(curve.vec2_keys, time)
                              : SampleLinear(curve.vec2_keys, time);
      reflect::SetVec2(target.field, target.component_ptr, val);
    } else if (!curve.vec3_keys.empty()) {
      glm::vec3 val = is_step ? *SampleStep(curve.vec3_keys, time)
                              : SampleLinear(curve.vec3_keys, time);
      reflect::SetVec3(target.field, target.component_ptr, val);
    } else if (!curve.vec4_keys.empty()) {
      glm::vec4 val = is_step ? *SampleStep(curve.vec4_keys, time)
                              : SampleLinear(curve.vec4_keys, time);
      reflect::SetVec4(target.field, target.component_ptr, val);
    } else if (!curve.quat_keys.empty()) {
      glm::quat val = is_step ? *SampleStep(curve.quat_keys, time)
                              : SampleLinear(curve.quat_keys, time);
      reflect::SetQuat(target.field, target.component_ptr, val);
    } else if (!curve.int_keys.empty()) {
      int val = *SampleStep(curve.int_keys, time);
      reflect::SetInt(target.field, target.component_ptr, val);
    } else if (!curve.bool_keys.empty()) {
      bool val = *SampleStep(curve.bool_keys, time);
      reflect::SetBool(target.field, target.component_ptr, val);
    } else if (!curve.asset_keys.empty()) {
      AssetHandle val = *SampleStep(curve.asset_keys, time);
      reflect::SetAssetHandle(target.field, target.component_ptr, val);
    }
  }
}

void BlendPropertyCurves(Scene& scene, entt::entity entity,
                         const std::vector<PropertyCurve>& curves_a,
                         float time_a,
                         const std::vector<PropertyCurve>& curves_b,
                         float time_b, float blend_weight) {
  // Build a lookup of curve B by target for matching
  std::unordered_map<std::string, const PropertyCurve*> b_lookup;
  for (const auto& cb : curves_b) {
    b_lookup[cb.target_component + "." + cb.target_field] = &cb;
  }

  // Evaluate curve B (the "new" clip) - this is the primary
  EvaluatePropertyCurves(scene, entity, curves_b, time_b);

  // For linear curves that also exist in A, blend the values
  for (const auto& ca : curves_a) {
    if (ca.interp != CurveInterp::Linear) {
      continue;
    }

    std::string key = ca.target_component + "." + ca.target_field;
    auto it = b_lookup.find(key);
    if (it == b_lookup.end()) {
      continue;
    }
    const PropertyCurve& cb = *it->second;
    if (cb.interp != CurveInterp::Linear) {
      continue;
    }

    ResolvedTarget target;
    if (!ResolveTarget(scene, entity, ca.target_component, ca.target_field,
                       target)) {
      continue;
    }

    if (!ca.float_keys.empty() && !cb.float_keys.empty()) {
      float va = SampleLinear(ca.float_keys, time_a);
      float vb = SampleLinear(cb.float_keys, time_b);
      reflect::SetFloat(target.field, target.component_ptr,
                        glm::mix(va, vb, blend_weight));
    } else if (!ca.vec3_keys.empty() && !cb.vec3_keys.empty()) {
      glm::vec3 va = SampleLinear(ca.vec3_keys, time_a);
      glm::vec3 vb = SampleLinear(cb.vec3_keys, time_b);
      reflect::SetVec3(target.field, target.component_ptr,
                       glm::mix(va, vb, blend_weight));
    } else if (!ca.vec4_keys.empty() && !cb.vec4_keys.empty()) {
      glm::vec4 va = SampleLinear(ca.vec4_keys, time_a);
      glm::vec4 vb = SampleLinear(cb.vec4_keys, time_b);
      reflect::SetVec4(target.field, target.component_ptr,
                       glm::mix(va, vb, blend_weight));
    } else if (!ca.quat_keys.empty() && !cb.quat_keys.empty()) {
      glm::quat va = SampleLinear(ca.quat_keys, time_a);
      glm::quat vb = SampleLinear(cb.quat_keys, time_b);
      reflect::SetQuat(target.field, target.component_ptr,
                       glm::slerp(va, vb, blend_weight));
    }
  }
}

}  // namespace wiesel
