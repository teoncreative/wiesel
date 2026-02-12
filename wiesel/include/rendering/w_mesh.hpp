
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

#include "asset/w_asset_handle.hpp"
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

  void Allocate();
  void Deallocate();

  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;
  std::string model_path;
  Ref<Material> mat;

  // Shared GPU geometry (created once, used by all entities referencing this mesh)
  Ref<MemoryBuffer> vertex_buffer;
  Ref<IndexBuffer> index_buffer;

  bool allocated_;
};

struct Model {
  Model() = default;
  ~Model() = default;

  std::vector<Ref<Mesh>> meshes;
  std::string model_path;
  std::string textures_path;
  std::map<std::string, Ref<Texture>> textures;
};

struct ModelComponent : public IComponent {
  ModelComponent() = default;
  // Copy settings but NOT render data - each entity gets its own allocation
  ModelComponent(const ModelComponent& other)
      : model_handle(other.model_handle),
        receive_shadows(other.receive_shadows),
        enable_rendering(other.enable_rendering) {}

  AssetHandle model_handle;
  bool receive_shadows = true;
  bool enable_rendering = true;

  // Per-entity render data (lazily allocated by renderer)
  Ref<UniformBuffer> uniform_buffer;
  std::vector<Ref<DescriptorSet>> geometry_descriptors;  // one per mesh
  std::vector<Ref<DescriptorSet>> shadow_descriptors;    // one per mesh
  AssetHandle render_model;  // tracks which model render data was built for
};
}  // namespace Wiesel
