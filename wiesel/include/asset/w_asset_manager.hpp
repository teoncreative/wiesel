//
// Created by Metehan Gezer on 10.02.2026.
//

#pragma once

#include "asset/w_asset_handle.hpp"
#include "asset/w_asset_loader.hpp"
#include "util/w_logger.hpp"
#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {

struct AssetMetadata {
  AssetHandle handle;
  AssetType type = AssetType::None;
  std::string name;
  std::string virtual_source_path;

  std::atomic<AssetLoadState> load_state = AssetLoadState::Unloaded;
  mutable std::atomic<float> load_progress{
      0.0f};  // 0.0-1.0 sub-progress within a single asset

  // Per-asset properties (type-erased, set during ScanAssets)
  std::shared_ptr<void> properties;

  template <typename T>
  T* GetProperties() const {
    return static_cast<T*>(properties.get());
  }

  template <typename T>
  T& GetOrCreateProperties() {
    if (!properties) {
      properties = std::make_shared<T>();
    }
    return *static_cast<T*>(properties.get());
  }

  bool IsValid() const { return handle.IsValid() && type != AssetType::None; }
};

class AssetManager {
 public:
  AssetManager() = default;

  // Registration (metadata only, no loading)

  AssetHandle Register(const std::string& name, AssetType type,
                       const std::string& virtual_source_path);

  bool Register(AssetHandle handle, const std::string& name, AssetType type,
                const std::string& virtual_source_path);

  void Unregister(AssetHandle handle);

  // Metadata queries

  bool HasAsset(AssetHandle handle) const;
  const AssetMetadata* GetMetadata(AssetHandle handle) const;

  AssetHandle FindByName(const std::string& name) const;
  AssetHandle FindBySourcePath(const std::string& virtual_source_path) const;

  std::vector<AssetHandle> GetAllOfType(AssetType type) const;
  std::vector<AssetHandle> GetAll() const;
  size_t GetAssetCount() const;

  struct AssetStats {
    size_t total = 0;
    size_t loaded = 0;
    size_t loading = 0;
    size_t unloaded = 0;
    size_t failed = 0;
  };

  AssetStats GetStats() const;

  // Load state
  bool SetLoadState(AssetHandle handle, AssetLoadState expected,
                    AssetLoadState new_state);
  AssetLoadState GetLoadState(AssetHandle handle) const;

  // Resource storage (type-erased)

  template <typename T>
  void Store(AssetHandle handle, std::shared_ptr<T> resource);

  template <typename T>
  std::shared_ptr<T> Get(AssetHandle handle) const;

  template <typename T>
  std::shared_ptr<T> GetOrLoad(AssetHandle handle) const;

  bool IsLoaded(AssetHandle handle) const;
  void Unload(AssetHandle handle);
  void UnloadAll();

  template <typename T>
  AssetHandle RegisterAndStore(const std::string& name, AssetType type,
                               const std::string& virtual_source_path,
                               std::shared_ptr<T> resource);

  // Asset loaders -/ register per-type loaders for sync/async loading
  void RegisterLoader(AssetType type, std::shared_ptr<IAssetLoader> loader);
  IAssetLoader* GetLoader(AssetType type) const;

  // Unified loading API
  // Sync: blocks until loaded. Async: returns immediately, loads in background.
  bool LoadSync(AssetHandle handle);
  void LoadAsync(AssetHandle handle);

  // Load all assets of a given type
  void LoadAllOfType(AssetType type, bool async = false);

  // Lifecycle

  void Clear();

 private:
  struct AssetEntry {
    AssetMetadata metadata;
    std::shared_ptr<void> resource;
  };

  std::unordered_map<AssetHandle, std::unique_ptr<AssetEntry>> registry_;
  std::unordered_map<std::string, AssetHandle> path_index_;
  std::unordered_map<std::string, AssetHandle> name_index_;
  std::unordered_map<AssetType, std::shared_ptr<IAssetLoader>> loaders_;
};

// Template implementations

template <typename T>
void AssetManager::Store(AssetHandle handle, std::shared_ptr<T> resource) {
  auto it = registry_.find(handle);
  if (it == registry_.end()) {
    LOG_ERROR("AssetManager::Store called with unregistered handle {}",
              handle.ToString());
    return;
  }
  it->second->resource = std::static_pointer_cast<void>(resource);
}

template <typename T>
std::shared_ptr<T> AssetManager::Get(AssetHandle handle) const {
  auto it = registry_.find(handle);
  if (it == registry_.end() || !it->second->resource) {
    return nullptr;
  }
  return std::static_pointer_cast<T>(it->second->resource);
}

template <typename T>
std::shared_ptr<T> AssetManager::GetOrLoad(AssetHandle handle) const {
  auto it = registry_.find(handle);
  if (it == registry_.end()) {
    return nullptr;
  }
  if (!it->second->resource) {
    // Trigger async load via the registered loader
    const_cast<AssetManager*>(this)->LoadAsync(handle);
    return nullptr;
  }
  return std::static_pointer_cast<T>(it->second->resource);
}

template <typename T>
AssetHandle AssetManager::RegisterAndStore(
    const std::string& name, AssetType type,
    const std::string& virtual_source_path, std::shared_ptr<T> resource) {
  AssetHandle handle = Register(name, type, virtual_source_path);
  if (handle.IsValid()) {
    Store<T>(handle, std::move(resource));
    SetLoadState(handle, AssetLoadState::Unloaded, AssetLoadState::Loaded);
  }
  return handle;
}

}  // namespace Wiesel
