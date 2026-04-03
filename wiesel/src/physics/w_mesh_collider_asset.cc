
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "physics/w_mesh_collider_asset.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

#include "asset/w_asset_manager.h"
#include "rendering/w_mesh.h"
#include "w_engine.h"

using namespace JPH;

namespace Wiesel {

std::shared_ptr<MeshColliderAssetData> BakeMeshColliderFromModel(
    AssetHandle model_handle) {
  if (!model_handle.IsValid()) {
    return nullptr;
  }

  auto model = Engine::asset_manager().Get<Model>(model_handle);
  if (!model) {
    Engine::asset_manager().LoadSync(model_handle);
    model = Engine::asset_manager().Get<Model>(model_handle);
  }
  if (!model) {
    LOG_ERROR("BakeMeshCollider: failed to load model {}",
              model_handle.ToString());
    return nullptr;
  }

  auto data = std::make_shared<MeshColliderAssetData>();
  data->source_model = model_handle;
  model->GetCollisionGeometry(data->vertices, data->indices);

  if (data->indices.empty()) {
    LOG_WARN("BakeMeshCollider: model has no collision geometry");
    return nullptr;
  }

  BuildCollisionShape(*data);

  LOG_INFO("Baked mesh collider: {} vertices, {} triangles",
           data->vertices.size(), data->indices.size() / 3);
  return data;
}

void BuildCollisionShape(MeshColliderAssetData& data) {
  if (data.indices.empty()) {
    return;
  }

  VertexList jolt_vertices;
  jolt_vertices.reserve(data.vertices.size());
  for (auto& p : data.vertices) {
    jolt_vertices.push_back(Float3(p.x, p.y, p.z));
  }

  IndexedTriangleList jolt_triangles;
  jolt_triangles.reserve(data.indices.size() / 3);
  for (size_t i = 0; i + 2 < data.indices.size(); i += 3) {
    jolt_triangles.push_back(IndexedTriangle(
        data.indices[i], data.indices[i + 1], data.indices[i + 2]));
  }

  MeshShapeSettings settings(std::move(jolt_vertices),
                             std::move(jolt_triangles));
  auto result = settings.Create();
  if (result.HasError()) {
    LOG_ERROR("BuildCollisionShape: failed to create MeshShape: {}",
              result.GetError().c_str());
    return;
  }

  // Wrap the Jolt ref-counted shape in a shared_ptr with custom deleter
  const Shape* raw = result.Get().GetPtr();
  raw->AddRef();
  data.cached_shape = std::shared_ptr<void>(
      const_cast<Shape*>(raw),
      [](void* ptr) { static_cast<Shape*>(ptr)->Release(); });
}

}  // namespace Wiesel
