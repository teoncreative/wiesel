//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "animation/w_animator.hpp"

#include "rendering/w_mesh.hpp"

namespace Wiesel {

// Binary search: find the index of the last key with time <= target
template <typename T>
static int FindKeyIndex(const std::vector<AnimationKey<T>>& keys, float time) {
  if (keys.size() <= 1) {
    return 0;
  }
  int lo = 0;
  int hi = static_cast<int>(keys.size()) - 1;
  while (lo < hi - 1) {
    int mid = (lo + hi) / 2;
    if (keys[mid].time <= time) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return lo;
}

glm::vec3 Animator::InterpolatePosition(const AnimationChannel& channel,
                                        float time) {
  auto& keys = channel.position_keys;
  if (keys.empty()) {
    return glm::vec3(0.0f);
  }
  if (keys.size() == 1) {
    return keys[0].value;
  }

  int i = FindKeyIndex(keys, time);
  int next = i + 1;
  if (next >= static_cast<int>(keys.size())) {
    return keys[i].value;
  }

  float dt = keys[next].time - keys[i].time;
  float t = (dt > 0.0f) ? (time - keys[i].time) / dt : 0.0f;
  t = glm::clamp(t, 0.0f, 1.0f);
  return glm::mix(keys[i].value, keys[next].value, t);
}

glm::quat Animator::InterpolateRotation(const AnimationChannel& channel,
                                        float time) {
  auto& keys = channel.rotation_keys;
  if (keys.empty()) {
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  }
  if (keys.size() == 1) {
    return keys[0].value;
  }

  int i = FindKeyIndex(keys, time);
  int next = i + 1;
  if (next >= static_cast<int>(keys.size())) {
    return keys[i].value;
  }

  float dt = keys[next].time - keys[i].time;
  float t = (dt > 0.0f) ? (time - keys[i].time) / dt : 0.0f;
  t = glm::clamp(t, 0.0f, 1.0f);
  return glm::slerp(keys[i].value, keys[next].value, t);
}

glm::vec3 Animator::InterpolateScale(const AnimationChannel& channel,
                                     float time) {
  auto& keys = channel.scale_keys;
  if (keys.empty()) {
    return glm::vec3(1.0f);
  }
  if (keys.size() == 1) {
    return keys[0].value;
  }

  int i = FindKeyIndex(keys, time);
  int next = i + 1;
  if (next >= static_cast<int>(keys.size())) {
    return keys[i].value;
  }

  float dt = keys[next].time - keys[i].time;
  float t = (dt > 0.0f) ? (time - keys[i].time) / dt : 0.0f;
  t = glm::clamp(t, 0.0f, 1.0f);
  return glm::mix(keys[i].value, keys[next].value, t);
}

glm::mat4 Animator::MakeTransform(const glm::vec3& pos, const glm::quat& rot,
                                  const glm::vec3& scale) {
  glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
  m *= glm::mat4_cast(rot);
  m = glm::scale(m, scale);
  return m;
}

void Animator::Evaluate(const Model& model, const AnimationClip& clip,
                        float time, std::vector<glm::mat4>& bone_matrices,
                        std::vector<glm::mat4>& node_transforms) {
  PROFILE_ZONE_SCOPED_N("Animator::Evaluate");
  const auto& hierarchy = model.node_hierarchy;
  const auto& skeleton = model.skeleton;

  // Build channel lookup (node_name -> channel index)
  std::unordered_map<std::string, int32_t> channel_map;
  channel_map.reserve(clip.channels.size());
  for (int32_t i = 0; i < static_cast<int32_t>(clip.channels.size()); i++) {
    channel_map[clip.channels[i].node_name] = i;
  }

  // Resize output arrays
  node_transforms.resize(hierarchy.nodes.size(), glm::mat4(1.0f));
  bone_matrices.resize(skeleton.bones.size(), glm::mat4(1.0f));

  // Walk hierarchy top-down to compute global transforms
  for (int32_t i = 0; i < static_cast<int32_t>(hierarchy.nodes.size()); i++) {
    const auto& node = hierarchy.nodes[i];

    // Determine local transform: animated or default
    glm::mat4 local_transform = node.local_transform;
    auto ch_it = channel_map.find(node.name);
    if (ch_it != channel_map.end()) {
      const auto& channel = clip.channels[ch_it->second];
      glm::vec3 pos = InterpolatePosition(channel, time);
      glm::quat rot = InterpolateRotation(channel, time);
      glm::vec3 scl = InterpolateScale(channel, time);
      local_transform = MakeTransform(pos, rot, scl);
    }

    // Compute global transform
    if (node.parent_index >= 0) {
      node_transforms[i] = node_transforms[node.parent_index] * local_transform;
    } else {
      node_transforms[i] = local_transform;
    }
  }

  // Compute bone skinning matrices
  for (int32_t b = 0; b < static_cast<int32_t>(skeleton.bones.size()); b++) {
    const auto& bone = skeleton.bones[b];
    // Find the node corresponding to this bone
    auto node_it = hierarchy.node_name_to_index.find(bone.name);
    if (node_it != hierarchy.node_name_to_index.end()) {
      int32_t node_idx = node_it->second;
      bone_matrices[b] = node_transforms[node_idx] * bone.inverse_bind_matrix;
    }
  }
}

void Animator::BlendAndSkin(const Model& model,
                            const std::vector<glm::mat4>& node_a,
                            const std::vector<glm::mat4>& node_b, float t,
                            std::vector<glm::mat4>& out_bone_matrices,
                            std::vector<glm::mat4>& out_node_transforms) {
  PROFILE_ZONE_SCOPED_N("Animator::BlendAndSkin");
  const auto& hierarchy = model.node_hierarchy;
  const auto& skeleton = model.skeleton;

  size_t count = std::min(node_a.size(), node_b.size());
  out_node_transforms.resize(hierarchy.nodes.size(), glm::mat4(1.0f));
  out_bone_matrices.resize(skeleton.bones.size(), glm::mat4(1.0f));
  t = glm::clamp(t, 0.0f, 1.0f);

  // Blend node transforms (these are clean global TRS matrices, safe to decompose)
  for (size_t i = 0; i < count; i++) {
    glm::vec3 pos_a, pos_b, scale_a, scale_b, skew;
    glm::quat rot_a, rot_b;
    glm::vec4 perspective;

    glm::decompose(node_a[i], scale_a, rot_a, pos_a, skew, perspective);
    glm::decompose(node_b[i], scale_b, rot_b, pos_b, skew, perspective);

    glm::vec3 pos = glm::mix(pos_a, pos_b, t);
    glm::quat rot = glm::slerp(rot_a, rot_b, t);
    glm::vec3 scale = glm::mix(scale_a, scale_b, t);

    out_node_transforms[i] = MakeTransform(pos, rot, scale);
  }

  // Copy remaining nodes from whichever is larger
  for (size_t i = count; i < node_a.size(); i++) {
    out_node_transforms[i] = node_a[i];
  }

  // Recompute bone skinning matrices from blended node transforms
  for (int32_t b = 0; b < static_cast<int32_t>(skeleton.bones.size()); b++) {
    const auto& bone = skeleton.bones[b];
    auto node_it = hierarchy.node_name_to_index.find(bone.name);
    if (node_it != hierarchy.node_name_to_index.end()) {
      int32_t node_idx = node_it->second;
      out_bone_matrices[b] =
          out_node_transforms[node_idx] * bone.inverse_bind_matrix;
    }
  }
}

}  // namespace Wiesel
