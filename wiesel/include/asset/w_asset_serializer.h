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

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "asset/w_asset_handle.h"
#include "asset/w_asset_manager.h"
#include "w_engine.h"

namespace Wiesel {

struct AssetSerializerDesc {
  AssetType type;
  // Serialize a stored asset resource to JSON fields (excluding asset_handle).
  std::function<nlohmann::json(AssetHandle handle)> Serialize;
  // Deserialize JSON into a resource and store it in AssetManager.
  std::function<bool(AssetHandle handle, const nlohmann::json& j)> Deserialize;
};

class AssetSerializerRegistry {
 public:
  static void Register(AssetSerializerDesc desc);

  // Save an existing asset to disk. Serializes via the registered function,
  // injects "asset_handle", writes JSON to the asset's VFS path.
  static bool Save(AssetHandle handle);

  // Save to a specific VFS path (e.g. for newly created assets).
  static bool Save(AssetHandle handle, const std::string& vfs_path);

  // Load an asset from disk. Reads JSON from the asset's VFS path,
  // calls the registered Deserialize function.
  static bool Load(AssetHandle handle);

  // Create a new asset: generate handle, register in AssetManager,
  // store the data, serialize to JSON, and write to disk.
  template <typename T>
  static AssetHandle Create(const std::string& name, AssetType type,
                            const std::string& vfs_path,
                            std::shared_ptr<T> data);

  static bool HasSerializer(AssetType type);

 private:
  static std::vector<AssetSerializerDesc>& Registry();
  static AssetSerializerDesc* Find(AssetType type);
};

// Template implementation
template <typename T>
AssetHandle AssetSerializerRegistry::Create(const std::string& name,
                                            AssetType type,
                                            const std::string& vfs_path,
                                            std::shared_ptr<T> data) {
  auto& mgr = Engine::asset_manager();
  AssetHandle handle = mgr.Register(name, type, vfs_path);
  if (!handle.IsValid()) {
    return {};
  }
  mgr.Store<T>(handle, data);
  if (!Save(handle, vfs_path)) {
    return {};
  }
  return handle;
}

// Call once from Engine::InitEngine().
void InitializeAssetSerializers();

}  // namespace Wiesel
