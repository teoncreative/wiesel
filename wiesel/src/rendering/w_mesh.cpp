
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

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<Index> indices) {
  Mat = CreateReference<Material>();
  Vertices = vertices;
  Indices = indices;
  IsAllocated = false;
}

Mesh::~Mesh() {
  Deallocate();
}

void Mesh::UpdateUniformBuffer(TransformComponent& transform) const {
  if (!IsAllocated) {
    return;
  }

  Ref<CameraData> camera = Engine::GetRenderer()->m_CameraData;
  if (!camera) { [[unlikely]]
    return;
  }
  Wiesel::UniformBufferObject ubo{};
  ubo.ModelMatrix = transform.TransformMatrix;
  ubo.Scale = transform.Scale;
  ubo.NormalMatrix = transform.NormalMatrix;
  ubo.RotationMatrix = transform.RotationMatrix;
  ubo.CameraViewMatrix = camera->ViewMatrix;
  ubo.CameraProjection = camera->Projection;
  ubo.CameraPosition = camera->Position;

  memcpy(UniformBuffer->m_Buffer, &ubo, sizeof(ubo));
}

void Mesh::Allocate() {
  if (IsAllocated) {
    Deallocate();
  }

  VertexBuffer = Engine::GetRenderer()->CreateVertexBuffer(Vertices);
  IndexBuffer = Engine::GetRenderer()->CreateIndexBuffer(Indices);
  UniformBuffer = Engine::GetRenderer()->CreateUniformBuffer(
      sizeof(UniformBufferObject));
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
