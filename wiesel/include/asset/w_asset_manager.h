//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 10.02.2026.
//

#pragma once

#include <shared_mutex>
#include <typeindex>
#include <unordered_set>
#include "asset/w_asset_handle.h"
#include "util/w_command.h"
#include "util/w_logger.h"
#include "util/w_utils.h"
#include "w_engine.h"
#include "w_pch.h"

namespace wiesel {

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

  // Returns the resource if already loaded, otherwise kicks off an
  // async load on the thread pool and returns nullptr immediately.
  // The caller MUST handle null. Suitable for per-frame render code that
  // can't block on disk I/O.
  template <typename T>
  std::shared_ptr<T> GetOrStartLoad(AssetHandle handle) const;

  // Synchronously loads the asset (if not already loaded) and returns it.
  // Blocks the calling thread on disk + parsing. Suitable for one-shot
  // setup paths (scene load, asset inspector, cursor activation) where a
  // missed asset is worse than a stalled frame. Returns nullptr only when
  // the handle is invalid or its loader fails.
  template <typename T>
  std::shared_ptr<T> LoadAndGet(AssetHandle handle);

  // Per-asset version, bumped when the resource is replaced (e.g. on reload).
  // Used by TextureSlot to detect stale cached pointers.
  uint32_t GetVersion(AssetHandle handle) const;

  bool IsLoaded(AssetHandle handle) const;
  void Unload(AssetHandle handle);
  void UnloadAll();
  void ReloadAllOfType(AssetType type);

  // Dependency tracking: when parent is unloaded, all dependents are
  // unloaded too. Call this during asset loading to register that
  // `dependent` requires `parent` (e.g. sprite depends on texture).
  void AddDependency(AssetHandle dependent, AssetHandle parent);
  void RemoveDependencies(AssetHandle dependent);

  // Get all assets that depend on the given parent.
  std::vector<AssetHandle> GetDependents(AssetHandle parent) const;

  template <typename T>
  AssetHandle RegisterAndStore(const std::string& name, AssetType type,
                               const std::string& virtual_source_path,
                               std::shared_ptr<T> resource);

  template <typename T>
  bool RegisterAndStore(AssetHandle handle, const std::string& name,
                        AssetType type, const std::string& virtual_source_path,
                        std::shared_ptr<T> resource);


  // Unified loading API
  // Sync: blocks until loaded. Async: returns immediately, loads in background.
  bool LoadSync(AssetHandle handle);
  void LoadAsync(AssetHandle handle);

  // Load all assets of a given type
  void LoadAllOfType(AssetType type, bool async = false);

  // Asset observer callbacks
  using AssetCallback = std::function<void(AssetHandle, const AssetMetadata&)>;
  void OnAssetRegistered(AssetCallback callback);
  void OnAssetUnregistered(AssetCallback callback);

  // Lifecycle

  void Clear();

  // Unregister every asset whose virtual_source_path starts with `prefix`
  // (e.g. "app://"). Fires OnAssetUnregistered callbacks for each removed
  // asset. Also broadcasts AssetUnloadedEvent for loaded assets so observers
  // can release GPU resources (thumbnails, etc.).
  void ClearByPathPrefix(const std::string& prefix);

  // Block until every async load submitted via LoadAsync has finished.
  void WaitForAsyncLoads();

 private:
  struct AssetEntry {
    AssetMetadata metadata;
    // One resource per type. Lets a single asset hold both its parsed source
    // form and any derived state (e.g. a Sprite holds both SpriteAssetData
    // and SpriteGpuData) without one Store overwriting the other and turning
    // a later Get<X> into a reinterpret-cast of an unrelated Y.
    std::unordered_map<std::type_index, std::shared_ptr<void>> resources;
    uint32_t version = 0;

    bool HasAnyResource() const { return !resources.empty(); }
  };

  std::unordered_map<AssetHandle, std::unique_ptr<AssetEntry>> registry_;
  std::unordered_map<std::string, AssetHandle> path_index_;
  std::unordered_map<std::string, AssetHandle> name_index_;

  // Dependency graph: parent -> set of dependents
  std::unordered_map<AssetHandle, std::unordered_set<AssetHandle>>
      parent_to_dependents_;
  // Reverse: dependent -> set of parents
  std::unordered_map<AssetHandle, std::unordered_set<AssetHandle>>
      dependent_to_parents_;

  // Observer callbacks
  std::vector<AssetCallback> on_registered_callbacks_;
  std::vector<AssetCallback> on_unregistered_callbacks_;

  // Thread safety: protects registry_, path_index_, name_index_, dependency
  // maps, and entry fields (resource, version). Loaders and callbacks are
  // accessed outside the lock to avoid deadlocks.
  mutable std::shared_mutex registry_mutex_;

  // Counts async loads that have been submitted to the thread pool but have
  // not returned yet. WaitForAsyncLoads() blocks on in_flight_cv_ until this
  // drains to zero.
  std::atomic<uint32_t> in_flight_async_loads_{0};
  std::mutex in_flight_mutex_;
  std::condition_variable in_flight_cv_;

  // Flipped by WaitForAsyncLoads() before it blocks. LoadAsync() refuses new
  // submissions while set, and already-queued lambdas bail out early instead
  // of running their loader.
  std::atomic<bool> shutting_down_{false};
};

// Template implementations

template <typename T>
void AssetManager::Store(AssetHandle handle, std::shared_ptr<T> resource) {
  std::unique_lock lock(registry_mutex_);
  if (!handle.IsValid()) {
    DCON_LOG_ERROR("AssetManager::Store called with invalid handle");
    return;
  }
  auto it = registry_.find(handle);
  if (it == registry_.end()) {
    DCON_LOG_ERROR("AssetManager::Store called with unregistered handle {}",
                   handle.ToString());
    return;
  }
  it->second->resources[std::type_index(typeid(T))] =
      std::static_pointer_cast<void>(resource);
  it->second->version++;
}

template <typename T>
std::shared_ptr<T> AssetManager::Get(AssetHandle handle) const {
  std::shared_lock lock(registry_mutex_);
  if (!handle.IsValid()) {
    return nullptr;
  }
  auto it = registry_.find(handle);
  if (it == registry_.end()) {
    return nullptr;
  }
  auto rit = it->second->resources.find(std::type_index(typeid(T)));
  if (rit == it->second->resources.end()) {
    return nullptr;
  }
  return std::static_pointer_cast<T>(rit->second);
}

template <typename T>
std::shared_ptr<T> AssetManager::GetOrStartLoad(AssetHandle handle) const {
  {
    std::shared_lock lock(registry_mutex_);
    if (!handle.IsValid()) {
      return nullptr;
    }
    auto it = registry_.find(handle);
    if (it == registry_.end()) {
      return nullptr;
    }
    auto rit = it->second->resources.find(std::type_index(typeid(T)));
    if (rit != it->second->resources.end()) {
      return std::static_pointer_cast<T>(rit->second);
    }
  }
  // Trigger async load via the registered loader (outside lock)
  const_cast<AssetManager*>(this)->LoadAsync(handle);
  return nullptr;
}

template <typename T>
std::shared_ptr<T> AssetManager::LoadAndGet(AssetHandle handle) {
  if (!handle.IsValid()) {
    return nullptr;
  }
  LoadSync(handle);
  return Get<T>(handle);
}

template <typename T>
AssetHandle AssetManager::RegisterAndStore(
    const std::string& name, AssetType type,
    const std::string& virtual_source_path, std::shared_ptr<T> resource) {
  AssetHandle handle = Register(name, type, virtual_source_path);
  if (handle.IsValid()) {
    Store<T>(handle, std::move(resource));
    SetLoadState(handle, AssetLoadState::Unloaded, AssetLoadState::Loaded);
    Engine::vfs()->RegisterVirtualEntry(virtual_source_path);
  }
  return handle;
}

template <typename T>
bool AssetManager::RegisterAndStore(AssetHandle handle, const std::string& name,
                                    AssetType type,
                                    const std::string& virtual_source_path,
                                    std::shared_ptr<T> resource) {
  if (!Register(handle, name, type, virtual_source_path)) {
    return false;
  }
  Store<T>(handle, std::move(resource));
  SetLoadState(handle, AssetLoadState::Unloaded, AssetLoadState::Loaded);
  Engine::vfs()->RegisterVirtualEntry(virtual_source_path);
  return true;
}

}  // namespace wiesel
