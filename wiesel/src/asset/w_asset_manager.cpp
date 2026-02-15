//
// Created by Metehan Gezer on 10.02.2026.
//

#include "asset/w_asset_manager.hpp"

namespace Wiesel {

const char* AssetTypeToString(AssetType type) {
  switch (type) {
    case AssetType::None:     return "None";
    case AssetType::Texture:  return "Texture";
    case AssetType::Model:    return "Model";
    case AssetType::Material: return "Material";
    case AssetType::Shader:   return "Shader";
    case AssetType::Sprite:   return "Sprite";
    case AssetType::Skybox:   return "Skybox";
    case AssetType::Font:     return "Font";
    case AssetType::Script:   return "Script";
    default:                  return "Unknown";
  }
}

AssetType AssetTypeFromString(std::string_view s) {
  if (s == "Texture")  return AssetType::Texture;
  if (s == "Model")    return AssetType::Model;
  if (s == "Material") return AssetType::Material;
  if (s == "Shader")   return AssetType::Shader;
  if (s == "Sprite")   return AssetType::Sprite;
  if (s == "Skybox")   return AssetType::Skybox;
  if (s == "Font")     return AssetType::Font;
  if (s == "Script")   return AssetType::Script;
  return AssetType::None;
}

AssetManager AssetManager::instance_;

AssetManager& AssetManager::Get() {
  return instance_;
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
                            AssetType type, const std::string& virtual_source_path) {
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
  if (it == registry_.end()) return;

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
  if (it == registry_.end()) return nullptr;
  return &it->second->metadata;
}

AssetHandle AssetManager::FindByName(const std::string& name) const {
  auto it = name_index_.find(name);
  if (it != name_index_.end()) return it->second;
  return kNullAssetHandle;
}

AssetHandle AssetManager::FindBySourcePath(const std::string& virtual_source_path) const {
  auto it = path_index_.find(virtual_source_path);
  if (it != path_index_.end()) return it->second;
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

// Load state

bool AssetManager::SetLoadState(AssetHandle handle, AssetLoadState expected, AssetLoadState new_state) {
  auto it = registry_.find(handle);
  if (it != registry_.end()) {
    return it->second->metadata.load_state.compare_exchange_strong(expected, new_state);
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
  if (it != registry_.end()) {
    it->second->resource.reset();
  }
}

void AssetManager::UnloadAll() {
  for (auto& [handle, entry] : registry_) {
    entry->resource.reset();
  }
}

void AssetManager::Clear() {
  registry_.clear();
  path_index_.clear();
  name_index_.clear();
}

}  // namespace Wiesel
