
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

void Mesh::UpdateTransform(TransformComponent& transform) const {
  if (!IsAllocated) { [[unlikely]]
    return;
  }

  MatriciesUniformData matricies{};
  matricies.ModelMatrix = transform.TransformMatrix;
  matricies.Scale = transform.Scale;
  matricies.NormalMatrix = transform.NormalMatrix;
  matricies.RotationMatrix = transform.RotationMatrix;

  memcpy(UniformBuffer->m_Data, &matricies, sizeof(MatriciesUniformData));
}

void Mesh::Allocate() {
  if (IsAllocated) {
    Deallocate();
  }

  VertexBuffer = Engine::GetRenderer()->CreateVertexBuffer(Vertices);
  IndexBuffer = Engine::GetRenderer()->CreateIndexBuffer(Indices);
  UniformBuffer = Engine::GetRenderer()->CreateUniformBuffer(
      sizeof(MatriciesUniformData));
  Descriptors = Engine::GetRenderer()->CreateDescriptors(UniformBuffer, Mat);
  IsAllocated = true;
}

void Mesh::Deallocate() {
  if (!IsAllocated) {
    return;
  }

  Mat = nullptr;
  UniformBuffer = nullptr;
  Descriptors = nullptr;
  VertexBuffer = nullptr;
  IndexBuffer = nullptr;
  IsAllocated = false;
}

}  // namespace Wiesel
