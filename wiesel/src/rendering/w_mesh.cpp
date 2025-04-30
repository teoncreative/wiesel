
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
  Mat = CreateReference<Material>();
  IsAllocated = false;
}

Mesh::Mesh(std::vector<Vertex3D> vertices, std::vector<Index> indices) {
  Mat = CreateReference<Material>();
  Vertices = vertices;
  Indices = indices;
  IsAllocated = false;
}

Mesh::~Mesh() {
  Deallocate();
}

void Mesh::UpdateTransform(glm::mat4 transformMatrix, glm::mat3 normalMatrix) const {
  if (!IsAllocated) { [[unlikely]]
    return;
  }

  MatricesUniformData matrices{};
  matrices.ModelMatrix = transformMatrix;
  matrices.NormalMatrix = normalMatrix;

  memcpy(UniformBuffer->m_Data, &matrices, sizeof(MatricesUniformData));
}

void Mesh::Allocate() {
  if (IsAllocated) {
    Deallocate();
  }

  VertexBuffer = Engine::GetRenderer()->CreateVertexBuffer(Vertices);
  IndexBuffer = Engine::GetRenderer()->CreateIndexBuffer(Indices);
  UniformBuffer = Engine::GetRenderer()->CreateUniformBuffer(
      sizeof(MatricesUniformData));
  GeometryDescriptors =
      Engine::GetRenderer()->CreateMeshDescriptors(UniformBuffer, Mat);
  ShadowDescriptors =
      Engine::GetRenderer()->CreateShadowMeshDescriptors(UniformBuffer, Mat);
  IsAllocated = true;
}

void Mesh::Deallocate() {
  if (!IsAllocated) {
    return;
  }
  Mat = nullptr;
  UniformBuffer = nullptr;
  GeometryDescriptors = nullptr;
  ShadowDescriptors = nullptr;
  VertexBuffer = nullptr;
  IndexBuffer = nullptr;
  IsAllocated = false;
}

}  // namespace Wiesel
