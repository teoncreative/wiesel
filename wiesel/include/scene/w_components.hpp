
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <entt/entt.hpp>
#include "events/w_events.hpp"
#include "rendering/w_buffer.hpp"
#include "rendering/w_descriptor.hpp"
#include "rendering/w_texture.hpp"
#include "animation/w_animation_controller.hpp"
#include "ui/w_canvas.hpp"
#include "util/w_utils.hpp"
#include "util/w_uuid.hpp"
#include "w_pch.hpp"

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

struct TagComponent : public IComponent {
  TagComponent(const std::string& tag) : tag(tag) {}
  TagComponent() = default;
  TagComponent(const TagComponent&) = default;

  std::string tag;
};

struct TransformComponent : public IComponent {
  TransformComponent() = default;
  TransformComponent(const TransformComponent&) = default;

  glm::vec3 GetForward();
  glm::vec3 GetBackward();
  glm::vec3 GetLeft();
  glm::vec3 GetRight();
  glm::vec3 GetUp();
  glm::vec3 GetDown();

  void Move(float dx, float dy, float dz);

  void Move(const glm::vec3& delta) { Move(delta.x, delta.y, delta.z); }

  void SetPosition(float x, float y, float z);

  void SetPosition(const glm::vec3& pos) { SetPosition(pos.x, pos.y, pos.z); }

  void Rotate(float dx, float dy, float dz);

  void Rotate(const glm::vec3& delta) { Rotate(delta.x, delta.y, delta.z); }

  void SetRotation(float x, float y, float z);

  void SetRotation(const glm::vec3& rot) { SetRotation(rot.x, rot.y, rot.z); }

  void Resize(float dx, float dy, float dz);

  void Resize(const glm::vec3& delta) { Resize(delta.x, delta.y, delta.z); }

  void SetScale(float x, float y, float z);

  void SetScale(const glm::vec3& scale) { SetScale(scale.x, scale.y, scale.z); }

  glm::vec3 position = {0.0f, 0.0f, 0.0f};
  // rotation in degrees
  glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
  glm::vec3 scale = {1.0f, 1.0f, 1.0f};
  glm::vec3 pivot = {0.0f, 0.0f, 0.0f};

  bool is_changed = true;
  glm::mat4 transform_matrix = {};
  glm::mat3 normal_matrix = {};
};

struct RectangleTransformComponent : public IComponent {
  glm::vec2 position = {0.0f, 0.0f};
  float rotation = 0.0f;
  glm::vec2 size = {100.0f, 100.0f};
  glm::vec2 scale = {1.0f, 1.0f};
  AnchorPreset anchor = AnchorPreset::TopLeft;      // point on parent
  AnchorPreset pivot = AnchorPreset::TopLeft;       // point on self
  SizeMode size_mode_x = SizeMode::Fixed;
  SizeMode size_mode_y = SizeMode::Fixed;
  glm::vec4 padding = {0.0f, 0.0f, 0.0f, 0.0f};

  // Computed by layout system (screen-space pixels)
  glm::vec2 computed_position = {0.0f, 0.0f};
  glm::vec2 computed_size = {0.0f, 0.0f};
  int32_t draw_order = 0;  // tree-traversal order for correct z-ordering

  bool is_changed = true;
  bool is_driven = true;
};

// Animation playback state (per-entity)
struct AnimatorComponent : public IComponent {
  AnimatorComponent() = default;
  AnimatorComponent(const AnimatorComponent& other)
      : current_clip_name(other.current_clip_name),
        playback_time(other.playback_time),
        playback_speed(other.playback_speed),
        looping(other.looping),
        playing(other.playing),
        controller(other.controller),
        parameters(other.parameters),
        current_state_name(other.current_state_name) {}

  // --- Legacy single-clip mode ---
  std::string current_clip_name;
  float playback_time = 0.0f;
  float playback_speed = 1.0f;
  bool looping = true;
  bool playing = false;

  // --- Controller mode (optional, empty = legacy mode) ---
  AnimationController controller;
  std::unordered_map<std::string, AnimParam> parameters;

  // State machine runtime
  std::string current_state_name;
  float state_time = 0.0f;  // time in current state (ticks)

  // Crossfade
  bool is_blending = false;
  std::string prev_clip_name;
  float prev_clip_time = 0.0f;
  float blend_weight = 0.0f;       // 0.0 = fully prev, 1.0 = fully current
  float blend_duration = 0.25f;    // seconds
  float blend_elapsed = 0.0f;
  std::vector<glm::mat4> prev_bone_matrices;
  std::vector<glm::mat4> prev_node_transforms;

  // Computed each frame (CPU side)
  std::vector<glm::mat4> bone_matrices;
  std::vector<glm::mat4> node_transforms;

  // Bone overrides, applied after animation eval, before GPU upload
  struct BoneOverride {
    std::string bone_name;
    glm::quat additional_rotation = glm::quat(1, 0, 0, 0);
    bool enabled = false;
    int32_t cached_node_index = -1;
    int32_t cached_bone_index = -1;
  };
  std::vector<BoneOverride> bone_overrides;

  // GPU resources (per-entity, allocated lazily)
  std::shared_ptr<UniformBuffer> bone_ubo;

  // --- Parameter API ---
  void SetBool(const std::string& name, bool value);
  void SetInt(const std::string& name, int value);
  void SetFloat(const std::string& name, float value);
  void SetTrigger(const std::string& name);
  bool GetBool(const std::string& name) const;
  int GetInt(const std::string& name) const;
  float GetFloat(const std::string& name) const;

  // Direct state change with optional crossfade (bypasses transition conditions)
  void Play(const std::string& state_name, float blend_time = 0.25f);

  bool UseController() const { return !controller.IsEmpty(); }
};

}  // namespace Wiesel