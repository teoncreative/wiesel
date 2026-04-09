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

#include <filesystem>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include "asset/w_asset_handle.h"
#include "asset/w_asset_manager.h"
#include "w_engine.h"

namespace Wiesel {

struct AssetTypeDesc {
  // Loading
  std::function<bool(AssetHandle)> Load;
  std::function<void(AssetHandle)> Unload;  // null = default mgr.Unload()

  // JSON serialization (JSON-backed assets only)
  std::function<nlohmann::json(AssetHandle)> Serialize;
  std::function<bool(AssetHandle, const nlohmann::json&)> Deserialize;

  // Import properties (.meta file)
  std::function<std::shared_ptr<void>()> CreateProperties;
  std::function<nlohmann::json(const void*)> SerializeProperties;
  std::function<std::shared_ptr<void>(const nlohmann::json&)>
      DeserializeProperties;
  std::function<bool(void*)> RenderPropertiesImGui;

  // Editor
  std::function<bool(AssetHandle)> RenderAssetImGui;

  bool IsJsonAsset() const { return Serialize && Deserialize; }

  bool HasProperties() const { return CreateProperties != nullptr; }
};

class AssetRegistry {
 public:
  static void Register(AssetType type, AssetTypeDesc desc);
  static const AssetTypeDesc* Get(AssetType type);
  static bool HasLoader(AssetType type);
  static bool HasSerializer(AssetType type);
  static bool HasProperties(AssetType type);

  // --- JSON asset operations ---
  static bool Save(AssetHandle handle);
  static bool Save(AssetHandle handle, const std::string& vfs_path);
  static bool LoadJson(AssetHandle handle);

  template <typename T>
  static AssetHandle Create(const std::string& name, AssetType type,
                            const std::string& vfs_path,
                            std::shared_ptr<T> data);

  // --- Meta file operations ---
  struct MetaFileData {
    AssetHandle handle;
    nlohmann::json properties;
  };

  static MetaFileData ReadMetaFile(const std::filesystem::path& path);
  static MetaFileData ReadMetaFile(const nlohmann::json& j);
  static void WriteMetaFile(const std::filesystem::path& path,
                            const AssetHandle& handle, AssetType type,
                            const void* properties = nullptr);

 private:
  static std::unordered_map<AssetType, AssetTypeDesc>& Registry();
};

// Template implementation
template <typename T>
AssetHandle AssetRegistry::Create(const std::string& name, AssetType type,
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
  auto physical = Engine::vfs()->GetPhysicalPath(vfs_path);
  if (physical.has_value()) {
    std::filesystem::path meta_path = physical->string() + ".meta";
    WriteMetaFile(meta_path, handle, type);
  }
  return handle;
}

// Call once from Engine::InitEngine().
void InitializeAssetRegistry();

}  // namespace Wiesel
