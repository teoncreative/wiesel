
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_components.h"

namespace Wiesel {

glm::vec3 TransformComponent::GetForward() {
  return glm::normalize(transform_matrix_[2]);
}

glm::vec3 TransformComponent::GetBackward() {
  return -glm::normalize(transform_matrix_[2]);
}

glm::vec3 TransformComponent::GetLeft() {
  return -glm::normalize(transform_matrix_[0]);
}

glm::vec3 TransformComponent::GetRight() {
  return glm::normalize(transform_matrix_[0]);
}

glm::vec3 TransformComponent::GetUp() {
  return glm::normalize(transform_matrix_[1]);
}

glm::vec3 TransformComponent::GetDown() {
  return -glm::normalize(transform_matrix_[1]);
}

// --- AnimatorComponent ---

void AnimatorComponent::SetBool(const std::string& name, bool value) {
  state_machine.SetBool(name, value);
}

void AnimatorComponent::SetInt(const std::string& name, int value) {
  state_machine.SetInt(name, value);
}

void AnimatorComponent::SetFloat(const std::string& name, float value) {
  state_machine.SetFloat(name, value);
}

void AnimatorComponent::SetTrigger(const std::string& name) {
  state_machine.SetTrigger(name);
}

bool AnimatorComponent::GetBool(const std::string& name) const {
  return state_machine.GetBool(name);
}

int AnimatorComponent::GetInt(const std::string& name) const {
  return state_machine.GetInt(name);
}

float AnimatorComponent::GetFloat(const std::string& name) const {
  return state_machine.GetFloat(name);
}

void AnimatorComponent::Play(const std::string& state_name) {
  state_machine.current_state = state_name;
  state_machine.state_time = 0.0f;
}

void AnimatorComponent::Stop() {
  playing = false;
}

std::string AnimatorComponent::GetCurrentState() const {
  return state_machine.current_state;
}

}  // namespace Wiesel