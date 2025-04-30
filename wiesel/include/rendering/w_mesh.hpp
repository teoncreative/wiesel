
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

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>

#include "rendering/w_buffer.hpp"
#include "rendering/w_descriptor.hpp"
#include "rendering/w_material.hpp"
#include "rendering/w_texture.hpp"
#include "scene/w_components.hpp"
#include "w_pch.hpp"

namespace Wiesel {
struct Mesh {
  Mesh();
  Mesh(std::vector<Vertex3D> vertices, std::vector<Index> indices);
  ~Mesh();

  void UpdateTransform(glm::mat4 transformMatrix, glm::mat3 normalMatrix) const;
  void Allocate();
  void Deallocate();

  std::vector<Vertex3D> Vertices;
  std::vector<Index> Indices;
  std::string ModelPath;

  bool IsAllocated;
  // Render Data
  Ref<MemoryBuffer> VertexBuffer;
  Ref<MemoryBuffer> ShadowVertexBuffer;
  Ref<IndexBuffer> IndexBuffer;
  Ref<UniformBuffer> UniformBuffer;
  Ref<Material> Mat;

  Ref<DescriptorSet> GeometryDescriptors;
  Ref<DescriptorSet> ShadowDescriptors;
};

struct Model {
  Model() = default;
  ~Model() = default;

  std::vector<Ref<Mesh>> Meshes;
  std::string ModelPath;
  std::string TexturesPath;
  std::map<std::string, Ref<Texture>> Textures;
  bool ReceiveShadows = true;  // todo shadows
};

struct ModelComponent : public IComponent {
  ModelComponent() = default;
  ModelComponent(const ModelComponent&) = default;

  Model Data;
};
}  // namespace Wiesel
