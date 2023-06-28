
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

#include "rendering/w_buffer.hpp"
#include "rendering/w_descriptor.hpp"
#include "rendering/w_texture.hpp"
#include "util/w_utils.hpp"
#include "util/w_uuid.hpp"
#include "w_pch.hpp"
#include "events/w_events.hpp"
#include <entt/entt.hpp>

namespace Wiesel {
  struct EventHandlerComponent {
    virtual void OnEvent(Event&);
  };

  struct IdComponent {
    IdComponent(UUID id) : Id(id) {}
    IdComponent() = default;
    IdComponent(const IdComponent&) = default;

    UUID Id;
  };

  struct ParentComponent {
    ParentComponent(entt::entity parent) : Parent(parent) {}
    ParentComponent() = default;
    ParentComponent(const ParentComponent&) = default;

    entt::entity Parent;
  };

  struct TagComponent {
    TagComponent() = default;
    TagComponent(const TagComponent&) = default;

    std::string Tag;
  };

  struct TransformComponent {
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
    void SetPosition(glm::vec3& pos) { SetPosition(pos.x, pos.y, pos.z); }

    void Rotate(float dx, float dy, float dz);
    void Rotate(const glm::vec3& delta) { Rotate(delta.x, delta.y, delta.z); }
    void SetRotation(float x, float y, float z);
    void SetRotation(glm::vec3& rot) { SetRotation(rot.x, rot.y, rot.z); }

    void Resize(float dx, float dy, float dz);
    void Resize(const glm::vec3& delta) { Resize(delta.x, delta.y, delta.z); }
    void SetScale(float x, float y, float z);
    void SetScale(const glm::vec3& scale) { SetScale(scale.x, scale.y, scale.z); }

    void UpdateMatrices();

    glm::vec3 Position = {0.0f, 0.0f, 0.0f};
    glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};

    bool IsChanged = true;
    glm::mat4 TransformMatrix = {};
    glm::mat3 NormalMatrix = {};
    glm::mat4 RotationMatrix = {};
  };

  struct RectangleTransformComponent {

    glm::vec2 Position = {0.0f, 0.0f};
    glm::vec2 Rotation = {0.0f, 0.0f};
    glm::vec2 Size = {0.0f, 0.0f};
    glm::vec2 Scale = {1.0f, 1.0f};

    bool IsChanged = true;
    bool IsDriven = true;

  };

#define ALL_COMPONENT_TYPES IdComponent, TagComponent, TransformComponent, ModelComponent, LightDirectComponent, LightPointComponent, CameraComponent, BehaviorsComponent

}