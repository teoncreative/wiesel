
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

#include <entt/entt.hpp>
#include "animation/w_animation_clip_asset.h"
#include "animation/w_animation_controller.h"
#include "animation/w_state_machine.h"
#include "core/w_reflect.h"
#include "events/w_events.h"
#include "math/w_aabb.h"
#include "rendering/w_buffer.h"
#include "rendering/w_descriptor.h"
#include "rendering/w_texture.h"
#include "ui/w_canvas.h"
#include "util/w_utils.h"
#include "util/w_uuid.h"
#include "w_pch.h"

namespace Wiesel {
struct EventHandlerComponent {
  virtual void OnEvent(Event&);
};

struct IComponent {
  virtual ~IComponent() = default;
};

struct IdComponent : public IComponent {
  IdComponent(UUID id) : Id(id) {}

  IdComponent() = default;
  IdComponent(const IdComponent&) = default;

  UUID Id;
};

struct TreeComponent : public IComponent {
  TreeComponent() = default;
  TreeComponent(const TreeComponent&) = default;

  entt::entity parent = entt::null;
  std::vector<entt::entity> childs;
};

WCLASS()

struct TagComponent : public IComponent {
  TagComponent(const std::string& name) : name(name) {}

  TagComponent() = default;
  TagComponent(const TagComponent&) = default;

  WPROPERTY(Serializable)
  std::string name;  // entity name
  WPROPERTY(Serializable)
  std::vector<std::string> tags;  // game tags ("Enemy", "Player", etc.)

  bool HasTag(const std::string& tag) const {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
  }

  void AddTag(const std::string& tag) {
    if (!HasTag(tag)) {
      tags.push_back(tag);
    }
  }

  void RemoveTag(const std::string& tag) {
    tags.erase(std::ranges::remove(tags, tag).begin(), tags.end());
  }
};

struct TransformComponent : public IComponent {
  TransformComponent() = default;
  TransformComponent(const TransformComponent&) = default;

  // Direction vectors (from cached transform matrix)
  glm::vec3 GetForward();
  glm::vec3 GetBackward();
  glm::vec3 GetLeft();
  glm::vec3 GetRight();
  glm::vec3 GetUp();
  glm::vec3 GetDown();

  // Getters (read-only, no dirty flag)
  const glm::vec3& GetPosition() const { return position_; }

  const glm::vec3& GetRotation() const { return rotation_; }

  const glm::vec3& GetScale() const { return scale_; }

  const glm::vec3& GetPivot() const { return pivot_; }

  bool IsChanged() const { return is_changed_; }

  const glm::mat4& GetTransformMatrix() const { return transform_matrix_; }

  const glm::mat3& GetNormalMatrix() const { return normal_matrix_; }

  // Setters (mark dirty automatically)
  void SetPosition(const glm::vec3& pos) {
    position_ = pos;
    is_changed_ = true;
  }

  void SetPosition(float x, float y, float z) {
    position_ = {x, y, z};
    is_changed_ = true;
  }

  void SetRotation(const glm::vec3& rot) {
    rotation_ = rot;
    is_changed_ = true;
  }

  void SetRotation(float x, float y, float z) {
    rotation_ = {x, y, z};
    is_changed_ = true;
  }

  void SetScale(const glm::vec3& scale) {
    scale_ = scale;
    is_changed_ = true;
  }

  void SetScale(float x, float y, float z) {
    scale_ = {x, y, z};
    is_changed_ = true;
  }

  void SetPivot(const glm::vec3& pivot) {
    pivot_ = pivot;
    is_changed_ = true;
  }

  // World-space getters (from cached transform matrix)
  glm::vec3 GetWorldPosition() const { return glm::vec3(transform_matrix_[3]); }

  glm::vec3 GetWorldScale() const {
    return {glm::length(glm::vec3(transform_matrix_[0])),
            glm::length(glm::vec3(transform_matrix_[1])),
            glm::length(glm::vec3(transform_matrix_[2]))};
  }

  // Direction conversion between local and world space
  glm::vec3 LocalToWorldDirection(const glm::vec3& dir) const {
    return glm::normalize(glm::mat3(transform_matrix_) * dir);
  }

  glm::vec3 WorldToLocalDirection(const glm::vec3& dir) const {
    return glm::normalize(glm::inverse(glm::mat3(transform_matrix_)) * dir);
  }

  // Point conversion between local and world space
  glm::vec3 LocalToWorldPoint(const glm::vec3& point) const {
    return glm::vec3(transform_matrix_ * glm::vec4(point, 1.0f));
  }

  glm::vec3 WorldToLocalPoint(const glm::vec3& point) const {
    return glm::vec3(glm::inverse(transform_matrix_) * glm::vec4(point, 1.0f));
  }

  enum class Space { Local, World };

  // Relative modifiers
  void Move(const glm::vec3& delta) {
    position_ += delta;
    is_changed_ = true;
  }

  void Move(float dx, float dy, float dz) { Move({dx, dy, dz}); }

  // Translate with space selection
  void Translate(const glm::vec3& delta, Space space = Space::Local) {
    if (space == Space::Local) {
      glm::mat3 rot = glm::mat3(transform_matrix_);
      // Strip scale by normalizing each column
      rot[0] = glm::normalize(rot[0]);  // local Right
      rot[1] = glm::normalize(rot[1]);  // local Up
      rot[2] = glm::normalize(rot[2]);  // local Forward
      position_ += rot * delta;
    } else {
      position_ += delta;
    }
    is_changed_ = true;
  }

  void Rotate(const glm::vec3& delta) {
    rotation_ += delta;
    is_changed_ = true;
  }

  void Rotate(float dx, float dy, float dz) { Rotate({dx, dy, dz}); }

  void Resize(const glm::vec3& delta) {
    scale_ += delta;
    is_changed_ = true;
  }

  void Resize(float dx, float dy, float dz) { Resize({dx, dy, dz}); }

  // Internal: used by scene to update cached matrices and reset flag
  void SetTransformMatrix(const glm::mat4& mat) {
    transform_matrix_ = mat;
    normal_matrix_ = glm::mat3(mat);
  }

  void ClearChanged() { is_changed_ = false; }

  void MarkChanged() { is_changed_ = true; }

  // Direct mutable access (for serialization, physics sync, editor inspector)
  glm::vec3& PositionMut() { return position_; }

  glm::vec3& RotationMut() { return rotation_; }

  glm::vec3& ScaleMut() { return scale_; }

  glm::vec3& PivotMut() { return pivot_; }

 private:
  glm::vec3 position_ = {0.0f, 0.0f, 0.0f};
  glm::vec3 rotation_ = {0.0f, 0.0f, 0.0f};  // in degrees
  glm::vec3 scale_ = {1.0f, 1.0f, 1.0f};
  glm::vec3 pivot_ = {0.0f, 0.0f, 0.0f};
  bool is_changed_ = true;
  glm::mat4 transform_matrix_ = {};
  glm::mat3 normal_matrix_ = {};
};

struct RectangleTransformComponent : public IComponent {
  glm::vec2 position = {0.0f, 0.0f};
  float rotation = 0.0f;
  glm::vec2 size = {100.0f, 100.0f};
  glm::vec2 scale = {1.0f, 1.0f};
  AnchorPreset anchor = AnchorPreset::TopLeft;  // point on parent
  AnchorPreset pivot = AnchorPreset::TopLeft;   // point on self
  SizeMode size_mode_x = SizeMode::Fixed;
  SizeMode size_mode_y = SizeMode::Fixed;
  glm::vec4 padding = {0.0f, 0.0f, 0.0f,
                       0.0f};  // inner padding (left, top, right, bottom)
  glm::vec4 margin = {0.0f, 0.0f, 0.0f,
                      0.0f};  // outer margin (left, top, right, bottom)

  // Computed by layout system (screen-space pixels)
  glm::vec2 computed_position = {0.0f, 0.0f};
  glm::vec2 computed_size = {0.0f, 0.0f};
  int32_t draw_order = 0;  // tree-traversal order for correct z-ordering

  void MarkChanged() { /* no op for now */ }
};

// Unified animation component - drives both skeletal and sprite animation
// via a shared controller asset (.wanimcontroller). The animation system
// auto-emplaces SkeletalAnimRuntime or SpriteAnimRuntime based on what
// other components the entity has.
WCLASS()

struct AnimatorComponent : public IComponent {
  AnimatorComponent() = default;

  WPROPERTY(Serializable)
  AssetHandle controller_handle;  // -> .wanimcontroller

  StateMachineRuntime state_machine;  // runtime state (not serialized)

  WPROPERTY(Serializable)
  float playback_speed = 1.0f;
  WPROPERTY(Serializable)
  bool playing = true;

  // Parameter API (delegates to state_machine)
  void SetBool(const std::string& name, bool value);
  void SetInt(const std::string& name, int value);
  void SetFloat(const std::string& name, float value);
  void SetTrigger(const std::string& name);
  bool GetBool(const std::string& name) const;
  int GetInt(const std::string& name) const;
  float GetFloat(const std::string& name) const;

  // Direct state change
  void Play(const std::string& state_name);
  void Stop();
  std::string GetCurrentState() const;
};

// Transient runtime data for skeletal animation. Emplaced automatically
// by AnimationSystem when the entity has AnimatorComponent.
struct SkeletalAnimRuntime {
  // Crossfade blending
  bool is_blending = false;
  std::string prev_clip_name;
  float prev_clip_time = 0.0f;
  float blend_weight = 0.0f;
  float blend_duration = 0.25f;
  float blend_elapsed = 0.0f;
  std::vector<glm::mat4> prev_bone_matrices;
  std::vector<glm::mat4> prev_node_transforms;

  // Computed each frame
  std::vector<glm::mat4> bone_matrices;
  std::vector<glm::mat4> node_transforms;

  // Max distance any bone reaches from origin in the current clip.
  // Used to expand frustum culling bounds.
  float max_bone_reach = 0.0f;

  // Bone overrides, applied after animation eval, before GPU upload
  struct BoneOverride {
    std::string bone_name;
    glm::quat additional_rotation = glm::quat(1, 0, 0, 0);
    bool enabled = false;
    int32_t cached_node_index = -1;
    int32_t cached_bone_index = -1;
  };

  std::vector<BoneOverride> bone_overrides;

  // GPU resources for bone matrices (shared by all skinned meshes referencing
  // this entity as skeleton_root). Allocated lazily by the renderer.
  std::shared_ptr<UniformBuffer> bone_ubo;
  std::shared_ptr<DescriptorSet> bone_descriptor;

  // Rest pose AABB computed from skinned vertices with bone matrices applied.
  // Used for frustum culling and debug bounds visualization.
  AABB rest_pose_bounds;

  // The model asset handle (needed for skeleton data access).
  AssetHandle model_handle;

  // Whether Initialize() has been called.
  bool initialized = false;

  // One-time setup: compute rest pose bone matrices, create bone UBO,
  // compute rest pose AABB. Call once after model_handle is set.
  void Initialize();
};

// Transient runtime data for sprite animation. Emplaced automatically
// by AnimationSystem when the entity has AnimatorComponent + SpriteRendererComponent.
struct SpriteAnimRuntime {
  uint32_t current_frame_index = 0;
  float frame_timer = 0.0f;
  std::shared_ptr<AnimClipAssetData> current_clip;
};

}  // namespace Wiesel