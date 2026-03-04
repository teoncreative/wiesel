
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_components.hpp"

namespace Wiesel {

glm::vec3 TransformComponent::GetForward() {
  return -transform_matrix[2];
}

glm::vec3 TransformComponent::GetBackward() {
  return transform_matrix[2];
}

glm::vec3 TransformComponent::GetLeft() {
  return -transform_matrix[0];
}

glm::vec3 TransformComponent::GetRight() {
  return transform_matrix[0];
}

glm::vec3 TransformComponent::GetUp() {
  return transform_matrix[1];
}

glm::vec3 TransformComponent::GetDown() {
  return -transform_matrix[1];
}

void TransformComponent::Move(float dx, float dy, float dz) {
  position += glm::vec3{dx, dy, dz};
  is_changed = true;
}

void TransformComponent::SetPosition(float x, float y, float z) {
  position = glm::vec3{x, y, z};
  is_changed = true;
}

void TransformComponent::Rotate(float dx, float dy, float dz) {
  rotation += glm::vec3{dx, dy, dz};
  is_changed = true;
}

void TransformComponent::SetRotation(float x, float y, float z) {
  rotation = glm::vec3{x, y, z};
  is_changed = true;
}

void TransformComponent::Resize(float dx, float dy, float dz) {
  scale += glm::vec3{dx, dy, dz};
  is_changed = true;
}

void TransformComponent::SetScale(float x, float y, float z) {
  scale = glm::vec3{x, y, z};
  is_changed = true;
}

// --- AnimatorComponent ---

void AnimatorComponent::SetBool(const std::string& name, bool value) {
  auto it = parameters.find(name);
  if (it != parameters.end() && it->second.type == AnimParamType::Bool) {
    it->second.b = value;
  }
}

void AnimatorComponent::SetInt(const std::string& name, int value) {
  auto it = parameters.find(name);
  if (it != parameters.end() && it->second.type == AnimParamType::Int) {
    it->second.i = value;
  }
}

void AnimatorComponent::SetFloat(const std::string& name, float value) {
  auto it = parameters.find(name);
  if (it != parameters.end() && it->second.type == AnimParamType::Float) {
    it->second.f = value;
  }
}

void AnimatorComponent::SetTrigger(const std::string& name) {
  auto it = parameters.find(name);
  if (it != parameters.end() && it->second.type == AnimParamType::Trigger) {
    it->second.b = true;
  }
}

bool AnimatorComponent::GetBool(const std::string& name) const {
  auto it = parameters.find(name);
  if (it != parameters.end() && it->second.type == AnimParamType::Bool) {
    return it->second.b;
  }
  return false;
}

int AnimatorComponent::GetInt(const std::string& name) const {
  auto it = parameters.find(name);
  if (it != parameters.end() && it->second.type == AnimParamType::Int) {
    return it->second.i;
  }
  return 0;
}

float AnimatorComponent::GetFloat(const std::string& name) const {
  auto it = parameters.find(name);
  if (it != parameters.end() && it->second.type == AnimParamType::Float) {
    return it->second.f;
  }
  return 0.0f;
}

void AnimatorComponent::Play(const std::string& state_name, float blend_time) {
  if (!UseController()) return;
  const auto* state = controller.FindState(state_name);
  if (!state) return;
  if (current_state_name == state_name && !is_blending) return;

  // Start crossfade from current to new state
  if (blend_time > 0.0f && !current_state_name.empty()) {
    // Save current pose as previous
    const auto* cur_state = controller.FindState(current_state_name);
    if (cur_state) {
      prev_clip_name = cur_state->clip_name;
      prev_clip_time = state_time;
      prev_bone_matrices = bone_matrices;
      prev_node_transforms = node_transforms;
      is_blending = true;
      blend_duration = blend_time;
      blend_elapsed = 0.0f;
      blend_weight = 0.0f;
    }
  }

  current_state_name = state_name;
  state_time = 0.0f;
}

}  // namespace Wiesel