
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
    return -TransformMatrix[2];
  }

  glm::vec3 TransformComponent::GetBackward() {
    return TransformMatrix[2];
  }

  glm::vec3 TransformComponent::GetLeft() {
    return -TransformMatrix[0];
  }

  glm::vec3 TransformComponent::GetRight() {
    return TransformMatrix[0];
  }

  glm::vec3 TransformComponent::GetUp() {
    return TransformMatrix[1];
  }

  glm::vec3 TransformComponent::GetDown() {
    return -TransformMatrix[1];
  }

  void TransformComponent::Move(float dx, float dy, float dz) {
    Position += glm::vec3{dx, dy, dz};
    IsChanged = true;
  }

  void TransformComponent::SetPosition(float x, float y, float z) {
    Position = glm::vec3{x, y, z};
    IsChanged = true;
  }

  void TransformComponent::Rotate(float dx, float dy, float dz) {
    Rotation += glm::vec3{dx, dy, dz};
    IsChanged = true;
  }

  void TransformComponent::SetRotation(float x, float y, float z) {
    Rotation = glm::vec3{x, y, z};
    IsChanged = true;
  }

  void TransformComponent::Resize(float dx, float dy, float dz) {
    Scale += glm::vec3{dx, dy, dz};
    IsChanged = true;
  }

  void TransformComponent::SetScale(float x, float y, float z) {
    Scale = glm::vec3{x, y, z};
    IsChanged = true;
  }

  void TransformComponent::UpdateMatrices() {
    RotationMatrix = glm::toMat4(glm::quat(Rotation));
    TransformMatrix = glm::translate(glm::mat4(1.0f), Position) * RotationMatrix * glm::scale(glm::mat4(1.0f), Scale);

    NormalMatrix = glm::inverseTranspose(glm::mat3(TransformMatrix));
  }

}// namespace Wiesel