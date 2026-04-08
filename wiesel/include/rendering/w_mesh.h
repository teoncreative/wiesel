
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <stb_image.h>
#include <assimp/Importer.hpp>

#include "animation/w_animation.h"
#include "asset/w_asset_handle.h"
#include "math/w_aabb.h"
#include "rendering/w_buffer.h"
#include "rendering/w_descriptor.h"
#include "rendering/w_material.h"
#include "rendering/w_texture.h"
#include "scene/w_components.h"
#include "util/w_logger.h"
#include "w_pch.h"

namespace Wiesel {

struct Mesh {
  Mesh();
  Mesh(const std::vector<Vertex3D>& vertices,
       const std::vector<Index>& indices);
  ~Mesh();

  void Allocate();
  void Deallocate();
  void ComputeBounds();

  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;
  AABB bounds;
  std::string model_path;
  std::shared_ptr<Material> mat;  // cached material pointer (set during import)
  AssetHandle material_handle;    // asset handle for this mesh's material
  int32_t node_index = -1;  // which node in the hierarchy this mesh belongs to

  // Shared GPU geometry (created once, used by all entities referencing this mesh)
  std::shared_ptr<MemoryBuffer> vertex_buffer;
  std::shared_ptr<IndexBuffer> index_buffer;

  bool has_transparency = false;
  bool allocated_;
};

// Pre-decoded texture data for parallel loading
struct DecodedTextureData {
  stbi_uc* pixels = nullptr;
  int width = 0;
  int height = 0;
  int channels = 0;

  bool has_semi_transparency = false;

  ~DecodedTextureData() {
    if (pixels) {
      stbi_image_free(pixels);
    }
  }

  DecodedTextureData() = default;

  DecodedTextureData(DecodedTextureData&& o) noexcept
      : pixels(o.pixels),
        width(o.width),
        height(o.height),
        channels(o.channels),
        has_semi_transparency(o.has_semi_transparency) {
    o.pixels = nullptr;
  }

  DecodedTextureData& operator=(DecodedTextureData&& o) noexcept {
    if (pixels) {
      stbi_image_free(pixels);
    }
    pixels = o.pixels;
    width = o.width;
    height = o.height;
    channels = o.channels;
    has_semi_transparency = o.has_semi_transparency;
    o.pixels = nullptr;
    return *this;
  }

  DecodedTextureData(const DecodedTextureData&) = delete;
  DecodedTextureData& operator=(const DecodedTextureData&) = delete;
};

struct Model {
  Model() = default;
  ~Model() = default;

  std::vector<std::shared_ptr<Mesh>> meshes;
  std::string model_path;
  std::string textures_path;

  // Pre-computed world transform per mesh (from node hierarchy)
  // Used for static models to position each mesh correctly
  std::vector<glm::mat4> mesh_node_transforms;

  // Animation data (shared across all entities using this model)
  Skeleton skeleton;
  NodeHierarchy node_hierarchy;
  std::vector<AnimationClip> animation_clips;
  bool has_skeleton = false;
  bool has_animations = false;
  bool has_transparent_meshes = false;

  AABB bounds;

  void ComputeBounds() {
    bounds = {};
    for (size_t i = 0; i < meshes.size(); i++) {
      meshes[i]->ComputeBounds();
      // Apply per-mesh node transform so bounds are in model space
      if (!has_skeleton && i < mesh_node_transforms.size()) {
        bounds.Expand(meshes[i]->bounds.Transformed(mesh_node_transforms[i]));
      } else {
        bounds.Expand(meshes[i]->bounds);
      }
    }
  }

  // Extract collision geometry with per-mesh node transforms applied.
  // Outputs positions and triangle indices suitable for physics or debug wireframe.
  void GetCollisionGeometry(std::vector<glm::vec3>& out_vertices,
                            std::vector<Index>& out_indices) const {
    uint32_t vertex_offset = 0;
    for (size_t mi = 0; mi < meshes.size(); mi++) {
      auto& mesh = meshes[mi];
      glm::mat4 node_xform = glm::mat4(1.0f);
      if (!has_skeleton && mi < mesh_node_transforms.size()) {
        node_xform = mesh_node_transforms[mi];
      }
      for (auto& v : mesh->vertices) {
        out_vertices.push_back(glm::vec3(node_xform * glm::vec4(v.ppos, 1.0f)));
      }
      for (size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
        out_indices.push_back(vertex_offset + mesh->indices[i]);
        out_indices.push_back(vertex_offset + mesh->indices[i + 1]);
        out_indices.push_back(vertex_offset + mesh->indices[i + 2]);
      }
      vertex_offset += static_cast<uint32_t>(mesh->vertices.size());
    }
  }

  // Compute mesh_node_transforms from node hierarchy (call after ProcessNode)
  void ComputeMeshNodeTransforms() {
    mesh_node_transforms.resize(meshes.size(), glm::mat4(1.0f));
    for (size_t i = 0; i < meshes.size(); i++) {
      int32_t ni = meshes[i]->node_index;
      if (ni >= 0) {
        mesh_node_transforms[i] = node_hierarchy.GetWorldTransform(ni);
      }
    }
  }
};

struct ModelComponent : public IComponent {
  ModelComponent() = default;

  // Copy settings but NOT render data. Each entity gets its own allocation
  ModelComponent(const ModelComponent& other)
      : model_handle(other.model_handle),
        receive_shadows(other.receive_shadows),
        enable_rendering(other.enable_rendering),
        material_slot_handles(other.material_slot_handles) {
    // Deep copy material instances
    material_instances.reserve(other.material_instances.size());
    for (const auto& inst : other.material_instances) {
      material_instances.push_back(
          inst ? std::make_shared<MaterialInstance>(*inst) : nullptr);
    }
  }

  AssetHandle model_handle;
  bool receive_shadows = true;
  bool enable_rendering = true;

  // Per-slot material handles (overrides mesh defaults, e.g. via drag-drop in editor)
  std::vector<AssetHandle> material_slot_handles;

  // Per-mesh material overrides (one per mesh slot, lazily created)
  std::vector<std::shared_ptr<MaterialInstance>> material_instances;

  // Cached material versions for descriptor invalidation
  std::vector<uint32_t> material_versions;

  // Per-entity render data (lazily allocated by renderer)
  std::shared_ptr<UniformBuffer> uniform_buffer;
  std::vector<std::shared_ptr<DescriptorSet>>
      geometry_descriptors;  // one per mesh
  std::vector<std::shared_ptr<DescriptorSet>>
      shadow_descriptors;    // one per mesh
  AssetHandle render_model;  // tracks which model render data was built for

  // Bone animation GPU data (per-entity)
  std::shared_ptr<UniformBuffer> bone_ubo_;
  std::shared_ptr<DescriptorSet> bone_descriptor_;

  // Per-mesh UBOs for node animation (only allocated for animated models)
  std::vector<std::shared_ptr<UniformBuffer>> mesh_uniform_buffers_;
};
}  // namespace Wiesel
