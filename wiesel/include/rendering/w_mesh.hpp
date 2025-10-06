
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
  Mesh(const std::vector<Vertex3D>& vertices, const std::vector<Index>& indices);
  ~Mesh();

  void UpdateTransform(glm::mat4 transform_matrix, glm::mat3 normal_matrix) const;
  void Allocate();
  void Deallocate();

  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;
  std::string model_path;

  bool allocated_;
  // Render Data
  Ref<MemoryBuffer> vertex_buffer;
  Ref<MemoryBuffer> shadow_vertex_buffer;
  Ref<IndexBuffer> index_buffer;
  Ref<UniformBuffer> uniform_buffer;
  Ref<Material> mat;

  Ref<DescriptorSet> geometry_descriptors;
  Ref<DescriptorSet> shadow_descriptors;
};

struct Model {
  Model() = default;
  ~Model() = default;

  std::vector<Ref<Mesh>> meshes;
  std::string model_path;
  std::string textures_path;
  std::map<std::string, Ref<Texture>> textures;
  bool receive_shadows = true;
  bool enable_rendering = true;
};

struct ModelComponent : public IComponent {
  ModelComponent() = default;
  ModelComponent(const ModelComponent&) = default;

  Model data;
};
}  // namespace Wiesel
