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

#include "asset/w_asset_manager.h"

#include "events/w_appevents.h"
#include "util/w_thread_pool.h"
#include "w_engine.h"

namespace Wiesel {

const char* AssetTypeToString(AssetType type) {
  switch (type) {
    case AssetType::None:
      return "None";
    case AssetType::Texture:
      return "Texture";
    case AssetType::Model:
      return "Model";
    case AssetType::Material:
      return "Material";
    case AssetType::Shader:
      return "Shader";
    case AssetType::Sprite:
      return "Sprite";
    case AssetType::Skybox:
      return "Skybox";
    case AssetType::Font:
      return "Font";
    case AssetType::Script:
      return "Script";
    case AssetType::Scene:
      return "Scene";
    case AssetType::Prefab:
      return "Prefab";
    case AssetType::Audio:
      return "Audio";
    case AssetType::SpriteAnim:
      return "SpriteAnim";
    case AssetType::SpriteController:
      return "SpriteController";
    case AssetType::UIDocument:
      return "UIDocument";
    case AssetType::UIStylesheet:
      return "UIStylesheet";
    default:
      return "Unknown";
  }
}

AssetType AssetTypeFromString(std::string_view s) {
  if (s == "Texture") {
    return AssetType::Texture;
  }
  if (s == "Model") {
    return AssetType::Model;
  }
  if (s == "Material") {
    return AssetType::Material;
  }
  if (s == "Shader") {
    return AssetType::Shader;
  }
  if (s == "Sprite") {
    return AssetType::Sprite;
  }
  if (s == "Skybox") {
    return AssetType::Skybox;
  }
  if (s == "Font") {
    return AssetType::Font;
  }
  if (s == "Script") {
    return AssetType::Script;
  }
  if (s == "Scene") {
    return AssetType::Scene;
  }
  if (s == "Prefab") {
    return AssetType::Prefab;
  }
  if (s == "Audio") {
    return AssetType::Audio;
  }
  if (s == "SpriteAnim") {
    return AssetType::SpriteAnim;
  }
  if (s == "SpriteController") {
    return AssetType::SpriteController;
  }
  if (s == "UIDocument") {
    return AssetType::UIDocument;
  }
  if (s == "UIStylesheet") {
    return AssetType::UIStylesheet;
  }
  return AssetType::None;
}

AssetHandle AssetManager::Register(const std::string& name, AssetType type,
                                   const std::string& virtual_source_path) {
  if (!virtual_source_path.empty()) {
    auto pit = path_index_.find(virtual_source_path);
    if (pit != path_index_.end()) {
      LOG_WARN("Asset with source path '{}' already registered as '{}'",
               virtual_source_path, registry_[pit->second]->metadata.name);
      return pit->second;
    }
  }

  AssetHandle handle = AssetHandle::Generate();

  auto entry = std::make_unique<AssetEntry>();
  entry->metadata.handle = handle;
  entry->metadata.type = type;
  entry->metadata.name = name;
  entry->metadata.virtual_source_path = virtual_source_path;

  registry_.emplace(handle, std::move(entry));

  if (!virtual_source_path.empty()) {
    path_index_[virtual_source_path] = handle;
  }
  if (!name.empty()) {
    name_index_.try_emplace(name, handle);
  }

  LOG_DEBUG("Registered asset '{}' [{}] path='{}' = {}", name,
            AssetTypeToString(type), virtual_source_path, handle.ToString());
  return handle;
}

bool AssetManager::Register(AssetHandle handle, const std::string& name,
                            AssetType type,
                            const std::string& virtual_source_path) {
  if (!handle.IsValid()) {
    LOG_ERROR("Cannot register asset with nil handle");
    return false;
  }
  if (registry_.contains(handle)) {
    LOG_WARN("Handle {} already registered", handle.ToString());
    return false;
  }

  auto entry = std::make_unique<AssetEntry>();
  entry->metadata.handle = handle;
  entry->metadata.type = type;
  entry->metadata.name = name;
  entry->metadata.virtual_source_path = virtual_source_path;

  registry_.emplace(handle, std::move(entry));

  if (!virtual_source_path.empty()) {
    path_index_.try_emplace(virtual_source_path, handle);
  }
  if (!name.empty()) {
    name_index_.try_emplace(name, handle);
  }

  return true;
}

void AssetManager::Unregister(AssetHandle handle) {
  auto it = registry_.find(handle);
  if (it == registry_.end()) {
    return;
  }

  const auto& meta = it->second->metadata;
  if (!meta.virtual_source_path.empty()) {
    path_index_.erase(meta.virtual_source_path);
  }
  if (!meta.name.empty()) {
    auto nit = name_index_.find(meta.name);
    if (nit != name_index_.end() && nit->second == handle) {
      name_index_.erase(nit);
    }
  }
  registry_.erase(it);
}

// Metadata queries

bool AssetManager::HasAsset(AssetHandle handle) const {
  return registry_.contains(handle);
}

const AssetMetadata* AssetManager::GetMetadata(AssetHandle handle) const {
  auto it = registry_.find(handle);
  if (it == registry_.end()) {
    return nullptr;
  }
  return &it->second->metadata;
}

AssetHandle AssetManager::FindByName(const std::string& name) const {
  auto it = name_index_.find(name);
  if (it != name_index_.end()) {
    return it->second;
  }
  return kNullAssetHandle;
}

AssetHandle AssetManager::FindBySourcePath(
    const std::string& virtual_source_path) const {
  auto it = path_index_.find(virtual_source_path);
  if (it != path_index_.end()) {
    return it->second;
  }
  return kNullAssetHandle;
}

std::vector<AssetHandle> AssetManager::GetAllOfType(AssetType type) const {
  std::vector<AssetHandle> result;
  for (const auto& [handle, entry] : registry_) {
    if (entry->metadata.type == type) {
      result.push_back(handle);
    }
  }
  return result;
}

std::vector<AssetHandle> AssetManager::GetAll() const {
  std::vector<AssetHandle> result;
  result.reserve(registry_.size());
  for (const auto& [handle, entry] : registry_) {
    result.push_back(handle);
  }
  return result;
}

size_t AssetManager::GetAssetCount() const {
  return registry_.size();
}

AssetManager::AssetStats AssetManager::GetStats() const {
  AssetStats stats;
  stats.total = registry_.size();
  for (const auto& [handle, entry] : registry_) {
    switch (entry->metadata.load_state.load()) {
      case AssetLoadState::Loaded:
        stats.loaded++;
        break;
      case AssetLoadState::Loading:
        stats.loading++;
        break;
      case AssetLoadState::Failed:
        stats.failed++;
        break;
      default:
        stats.unloaded++;
        break;
    }
  }
  return stats;
}

// Load state

bool AssetManager::SetLoadState(AssetHandle handle, AssetLoadState expected,
                                AssetLoadState new_state) {
  auto it = registry_.find(handle);
  if (it != registry_.end()) {
    return it->second->metadata.load_state.compare_exchange_strong(expected,
                                                                   new_state);
  }
  return false;
}

AssetLoadState AssetManager::GetLoadState(AssetHandle handle) const {
  auto it = registry_.find(handle);
  if (it != registry_.end()) {
    return it->second->metadata.load_state;
  }
  return AssetLoadState::Unloaded;
}

// Resource lifecycle

bool AssetManager::IsLoaded(AssetHandle handle) const {
  auto it = registry_.find(handle);
  return it != registry_.end() && it->second->resource != nullptr;
}

void AssetManager::Unload(AssetHandle handle) {
  auto it = registry_.find(handle);
  if (it != registry_.end() && it->second->resource) {
    it->second->resource.reset();
    it->second->metadata.load_state.store(AssetLoadState::Unloaded);
    it->second->metadata.load_progress.store(0.0f);

    AssetUnloadedEvent event(handle);
    Engine::BroadcastEvent(event);
  }

  // Cascade: unload all assets that depend on this one
  auto dep_it = parent_to_dependents_.find(handle);
  if (dep_it != parent_to_dependents_.end()) {
    auto dependents = dep_it->second;
    for (const auto& dep : dependents) {
      Unload(dep);
    }
  }
}

void AssetManager::UnloadAll() {
  for (auto& [handle, entry] : registry_) {
    entry->resource.reset();
  }
}

void AssetManager::AddDependency(AssetHandle dependent, AssetHandle parent) {
  if (!dependent.IsValid() || !parent.IsValid()) {
    return;
  }
  parent_to_dependents_[parent].insert(dependent);
  dependent_to_parents_[dependent].insert(parent);
}

void AssetManager::RemoveDependencies(AssetHandle dependent) {
  auto it = dependent_to_parents_.find(dependent);
  if (it == dependent_to_parents_.end()) {
    return;
  }
  for (const auto& parent : it->second) {
    auto p_it = parent_to_dependents_.find(parent);
    if (p_it != parent_to_dependents_.end()) {
      p_it->second.erase(dependent);
      if (p_it->second.empty()) {
        parent_to_dependents_.erase(p_it);
      }
    }
  }
  dependent_to_parents_.erase(it);
}

std::vector<AssetHandle> AssetManager::GetDependents(AssetHandle parent) const {
  auto it = parent_to_dependents_.find(parent);
  if (it == parent_to_dependents_.end()) {
    return {};
  }
  return {it->second.begin(), it->second.end()};
}

void AssetManager::Clear() {
  registry_.clear();
  path_index_.clear();
  name_index_.clear();
  parent_to_dependents_.clear();
  dependent_to_parents_.clear();
}

void AssetManager::RegisterLoader(AssetType type,
                                  std::shared_ptr<IAssetLoader> loader) {
  loaders_[type] = std::move(loader);
}

IAssetLoader* AssetManager::GetLoader(AssetType type) const {
  auto it = loaders_.find(type);
  if (it != loaders_.end()) {
    return it->second.get();
  }
  return nullptr;
}

bool AssetManager::LoadSync(AssetHandle handle) {
  auto it = registry_.find(handle);
  if (it == registry_.end()) {
    return false;
  }
  auto& meta = it->second->metadata;
  if (meta.load_state == AssetLoadState::Loaded) {
    return true;
  }
  auto* loader = GetLoader(meta.type);
  if (!loader) {
    LOG_WARN("No loader registered for asset type: {}",
             AssetTypeToString(meta.type));
    return false;
  }
  if (!SetLoadState(handle, AssetLoadState::Unloaded,
                    AssetLoadState::Loading)) {
    return false;
  }
  bool success = loader->Load(handle);
  if (success) {
    SetLoadState(handle, AssetLoadState::Loading, AssetLoadState::Loaded);
  } else {
    SetLoadState(handle, AssetLoadState::Loading, AssetLoadState::Failed);
  }
  return success;
}

void AssetManager::LoadAsync(AssetHandle handle) {
  auto it = registry_.find(handle);
  if (it == registry_.end()) {
    return;
  }
  auto& meta = it->second->metadata;
  if (meta.load_state == AssetLoadState::Loaded ||
      meta.load_state == AssetLoadState::Loading) {
    return;
  }
  auto* loader = GetLoader(meta.type);
  if (!loader) {
    LOG_WARN("No loader registered for asset type: {}",
             AssetTypeToString(meta.type));
    return;
  }
  if (!SetLoadState(handle, AssetLoadState::Unloaded,
                    AssetLoadState::Loading)) {
    return;
  }

  auto loader_ptr = loaders_[meta.type];
  Engine::thread_pool().Submit([this, handle, loader_ptr]() {
    bool success = loader_ptr->Load(handle);
    if (success) {
      SetLoadState(handle, AssetLoadState::Loading, AssetLoadState::Loaded);
    } else {
      SetLoadState(handle, AssetLoadState::Loading, AssetLoadState::Failed);
    }
  });
}

void AssetManager::LoadAllOfType(AssetType type, bool async) {
  for (auto& [handle, entry] : registry_) {
    if (entry->metadata.type == type &&
        entry->metadata.load_state == AssetLoadState::Unloaded) {
      if (async) {
        LoadAsync(handle);
      } else {
        LoadSync(handle);
      }
    }
  }
}

}  // namespace Wiesel
