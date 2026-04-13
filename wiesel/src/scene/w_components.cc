
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

#include "animation/w_animation.h"
#include "animation/w_animator.h"
#include "asset/w_asset_manager.h"
#include "rendering/w_mesh.h"
#include "rendering/w_renderer.h"
#include "w_engine.h"

namespace wiesel {

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

// --- SkeletalAnimRuntime ---

void SkeletalAnimRuntime::Initialize() {
  if (initialized || !model_handle.IsValid()) {
    return;
  }

  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }

  auto model_data = Engine::asset_manager().GetOrLoad<Model>(model_handle);
  if (!model_data) {
    return;
  }

  // Create bone UBO
  bone_ubo = renderer->CreateUniformBuffer("SkeletalAnimRuntime::bone_ubo",
                                           sizeof(BoneMatricesUniformData));

  // Compute rest pose from node hierarchy default transforms
  AnimationClip rest_clip;
  Animator::Evaluate(*model_data, rest_clip, 0.0f, bone_matrices,
                     node_transforms);

  // Upload rest pose to GPU
  if (bone_ubo->data_) {
    BoneMatricesUniformData gpu_data{};
    size_t count =
        std::min(bone_matrices.size(), static_cast<size_t>(WIESEL_MAX_BONES));
    for (size_t b = 0; b < count; b++) {
      gpu_data.bone_matrices[b] = bone_matrices[b];
    }
    memcpy(bone_ubo->data_, &gpu_data, sizeof(BoneMatricesUniformData));
  }

  // Create bone descriptor
  bone_descriptor = renderer->CreateBoneDescriptors(bone_ubo);

  // Compute rest pose AABB from skinned vertices
  rest_pose_bounds = {};
  for (const auto& mesh : model_data->meshes) {
    for (const auto& v : mesh->vertices) {
      glm::vec4 skinned_pos(0.0f);
      for (int w = 0; w < WIESEL_MAX_BONE_INFLUENCE; w++) {
        int bone_id = v.bone_indices[w];
        float weight = v.bone_weights[w];
        if (bone_id >= 0 && bone_id < static_cast<int>(bone_matrices.size()) &&
            weight > 0.0f) {
          skinned_pos +=
              bone_matrices[bone_id] * glm::vec4(v.ppos, 1.0f) * weight;
        }
      }
      rest_pose_bounds.Expand(glm::vec3(skinned_pos));
    }
  }

  initialized = true;
}

}  // namespace wiesel