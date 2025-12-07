
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

}  // namespace Wiesel