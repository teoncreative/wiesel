
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

  // Cameras and lights from the imported scene (e.g. FBX)
  struct ImportedCamera {
    std::string node_name;  // matches a node in the hierarchy
    float fov = 60.0f;      // vertical FOV in degrees
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float aspect_ratio = 0.0f;       // 0 = auto
    glm::vec3 look_at = {0, 0, -1};  // camera look direction (local)
    glm::vec3 up = {0, 1, 0};        // camera up vector (local)
  };

  struct ImportedLight {
    enum class Type { Directional, Point, Spot };
    std::string node_name;
    Type type = Type::Point;
    glm::vec3 color = {1, 1, 1};
    float intensity = 1.0f;
    // Point/spot
    float attenuation_constant = 1.0f;
    float attenuation_linear = 0.0f;
    float attenuation_quadratic = 0.0f;
    // Spot
    float inner_cone_angle = 0.0f;
    float outer_cone_angle = 0.0f;
  };

  std::vector<ImportedCamera> imported_cameras;
  std::vector<ImportedLight> imported_lights;

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

}  // namespace Wiesel
