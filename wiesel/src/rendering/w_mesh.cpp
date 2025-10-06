
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_mesh.hpp"

#include "rendering/w_renderer.hpp"
#include "w_engine.hpp"

namespace Wiesel {

Mesh::Mesh() {
  mat = CreateReference<Material>();
  allocated_ = false;
}

Mesh::Mesh(const std::vector<Vertex3D>& vertices, const std::vector<Index>& indices) : vertices(vertices), indices(indices) {
  mat = CreateReference<Material>();
  allocated_ = false;
}

Mesh::~Mesh() {
  Deallocate();
}

void Mesh::UpdateTransform(glm::mat4 transform_matrix, glm::mat3 normal_matrix) const {
  if (!allocated_) { [[unlikely]]
    return;
  }

  MatricesUniformData matrices{};
  matrices.ModelMatrix = transform_matrix;
  matrices.NormalMatrix = normal_matrix;

  memcpy(uniform_buffer->data_, &matrices, sizeof(MatricesUniformData));
}

void Mesh::Allocate() {
  if (allocated_) {
    Deallocate();
  }

  vertex_buffer = Engine::GetRenderer()->CreateVertexBuffer(vertices);
  index_buffer = Engine::GetRenderer()->CreateIndexBuffer(indices);
  uniform_buffer = Engine::GetRenderer()->CreateUniformBuffer(
      sizeof(MatricesUniformData));
  geometry_descriptors =
      Engine::GetRenderer()->CreateMeshDescriptors(uniform_buffer, mat);
  shadow_descriptors =
      Engine::GetRenderer()->CreateShadowMeshDescriptors(uniform_buffer, mat);
  allocated_ = true;
}

void Mesh::Deallocate() {
  if (!allocated_) {
    return;
  }
  mat = nullptr;
  uniform_buffer = nullptr;
  geometry_descriptors = nullptr;
  shadow_descriptors = nullptr;
  vertex_buffer = nullptr;
  index_buffer = nullptr;
  allocated_ = false;
}

}  // namespace Wiesel
