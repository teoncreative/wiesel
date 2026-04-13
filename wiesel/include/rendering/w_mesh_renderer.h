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

#include <entt/entt.hpp>

#include "asset/w_asset_handle.h"
#include "rendering/w_buffer.h"
#include "rendering/w_descriptor.h"
#include "rendering/w_material.h"

namespace wiesel {

// Per-entity component for rendering a single static mesh.
// Each mesh from an imported model gets its own entity with this component.
struct MeshRendererComponent {
  AssetHandle model_handle;  // shared Model asset (for mesh geometry access)
  int32_t mesh_index = -1;   // which mesh in the Model's meshes array

  bool enable_rendering = true;
  bool receive_shadows = true;

  // Material for this mesh
  AssetHandle material_handle;
  std::shared_ptr<MaterialInstance> material_instance;
  uint32_t material_version = 0;

  // Per-mesh GPU resources (lazily allocated by renderer)
  std::shared_ptr<UniformBuffer> ubo;
  std::shared_ptr<DescriptorSet> geometry_descriptor;
  std::shared_ptr<DescriptorSet> shadow_descriptor;
  bool gpu_allocated = false;
};

// Per-entity component for rendering a single skinned (bone-animated) mesh.
// References a skeleton root entity that owns the AnimatorComponent and bone UBO.
struct SkinnedMeshRendererComponent {
  AssetHandle model_handle;
  int32_t mesh_index = -1;

  bool enable_rendering = true;
  bool receive_shadows = true;

  // Material for this mesh
  AssetHandle material_handle;
  std::shared_ptr<MaterialInstance> material_instance;
  uint32_t material_version = 0;

  // Per-mesh GPU resources (lazily allocated by renderer)
  std::shared_ptr<UniformBuffer> ubo;
  std::shared_ptr<DescriptorSet> geometry_descriptor;
  std::shared_ptr<DescriptorSet> shadow_descriptor;
  bool gpu_allocated = false;

  // Reference to the root entity that owns AnimatorComponent + SkeletalAnimRuntime.
  // The bone UBO and descriptor live on that entity's SkeletalAnimRuntime.
  entt::entity skeleton_root = entt::null;
};

}  // namespace wiesel
