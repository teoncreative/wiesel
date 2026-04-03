
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

#include "asset/w_asset_handle.h"
#include "util/w_utils.h"
#include "w_pch.h"

namespace Wiesel {

struct MeshColliderAssetData {
  AssetHandle source_model;
  std::vector<glm::vec3> vertices;
  std::vector<uint32_t> indices;

  // Pre-built Jolt MeshShape (created during asset load, shared by all entities).
  // Stored as void* to avoid pulling Jolt headers into this header.
  // The actual type is JPH::RefConst<JPH::Shape> wrapped in shared_ptr.
  std::shared_ptr<void> cached_shape;
};

// Bake collision geometry from a model asset.
// Extracts all mesh vertices/indices via Model::GetCollisionGeometry().
// Returns nullptr if the model is not loaded or has no geometry.
std::shared_ptr<MeshColliderAssetData> BakeMeshColliderFromModel(
    AssetHandle model_handle);

// Pre-create the Jolt MeshShape from vertices/indices and store it in
// cached_shape. Called during asset load so the shape is ready immediately.
void BuildCollisionShape(MeshColliderAssetData& data);

}  // namespace Wiesel
