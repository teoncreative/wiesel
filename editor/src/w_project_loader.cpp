//
//   Copyright 2025 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_project_loader.hpp"

#include <nlohmann/json.hpp>

#include "asset/w_asset_manager.hpp"
#include "asset/w_asset_properties.hpp"
#include "asset/w_asset_property_registry.hpp"
#include "asset/w_asset_utils.hpp"
#include "input/w_input.hpp"
#include "rendering/w_material.hpp"
#include "rendering/w_renderer.hpp"
#include "scene/w_scene_manager.hpp"
#include "scene/w_scene_serializer.hpp"
#include "script/w_scriptmanager.hpp"
#include "w_engine.hpp"

namespace Wiesel {

void ProjectLoader::WriteMetaFile(const std::filesystem::path& meta_path,
                                  const AssetHandle& handle, AssetType type,
                                  const void* properties) {
  nlohmann::json j;
  j["handle"] = handle.ToString();
  if (properties) {
    const auto* desc = AssetPropertyRegistry::Get(type);
    if (desc) {
      j["properties"] = desc->Serialize(properties);
    }
  }
  std::ofstream file(meta_path);
  if (file.is_open()) {
    file << j.dump(2);
  }
}

AssetHandle ProjectLoader::ImportAsset(const std::string& name, AssetType type,
                                       const std::string& vfs_path) {
  AssetManager& mgr = Engine::asset_manager();
  AssetHandle handle;

  auto physical = Engine::vfs()->GetPhysicalPath(vfs_path);

  if (IsJsonAssetType(type)) {
    // JSON assets store their handle inside the file
    try {
      VfsFile file = Engine::vfs()->Open(vfs_path);
      if (!file) {
        return {};
      }
      std::string content((std::istreambuf_iterator<char>(file.Stream())),
                          std::istreambuf_iterator<char>());
      auto j = nlohmann::json::parse(content);

      std::string handle_str = j.value("asset_handle", "");
      if (!handle_str.empty()) {
        handle = AssetHandle::FromString(handle_str);
      }

      if (!handle.IsValid()) {
        if (!physical.has_value()) {
          return {};
        }
        handle = AssetHandle::Generate();
        j["asset_handle"] = handle.ToString();
        std::ofstream out(*physical);
        if (out.is_open()) {
          out << j.dump(2);
        }
      }
    } catch (const std::exception& e) {
      LOG_ERROR("Failed to import '{}': {}", vfs_path, e.what());
      return {};
    }

    if (!mgr.HasAsset(handle)) {
      mgr.Register(handle, name, type, vfs_path);
    }
  } else {
    // Binary assets use .meta sidecar files
    GameLoader::MetaFileData meta_data;
    std::filesystem::path meta_path;
    if (physical.has_value()) {
      meta_path = physical->string() + ".meta";
      meta_data = GameLoader::ReadMetaFile(meta_path);
    }

    if (meta_data.handle.IsValid()) {
      handle = meta_data.handle;
      if (!mgr.HasAsset(handle)) {
        mgr.Register(handle, name, type, vfs_path);
      }
    } else if (!physical.has_value()) {
      return {};
    } else {
      handle = mgr.Register(name, type, vfs_path);
      if (handle.IsValid()) {
        WriteMetaFile(meta_path, handle, type);
      }
    }

    // Set up asset properties from .meta or create defaults
    if (handle.IsValid()) {
      auto* metadata = const_cast<AssetMetadata*>(mgr.GetMetadata(handle));
      if (metadata) {
        const auto* desc = AssetPropertyRegistry::Get(type);
        if (desc) {
          if (!meta_data.properties.empty()) {
            metadata->properties = desc->Deserialize(meta_data.properties);
          } else {
            metadata->properties = desc->Create();
            if (type == AssetType::Texture) {
              auto* tp = static_cast<TextureAssetProperties*>(
                  metadata->properties.get());
              std::string nl = name;
              std::ranges::transform(nl, nl.begin(), ::tolower);
              if (nl.find("normal") != std::string::npos ||
                  nl.find("roughness") != std::string::npos ||
                  nl.find("metallic") != std::string::npos ||
                  nl.find("metalness") != std::string::npos ||
                  nl.find("height") != std::string::npos ||
                  nl.find("ao") != std::string::npos) {
                tp->asset_type = TextureAssetType::NormalMap;
              }
            }
          }
        }
      }
    }
  }

  return handle;
}

bool ProjectLoader::MountProject(Project& project) {
  return GameLoader::MountAssets(project.GetAssetsDirectory());
}

void ProjectLoader::ScanAssets(Project& project) {
  namespace fs = std::filesystem;
  AssetManager& mgr = Engine::asset_manager();
  Engine::scene_manager().ClearRegisteredScenes();
  std::vector<std::string> scenes_to_preload;

  fs::path assets_dir = fs::absolute(project.GetAssetsDirectory());
  if (!fs::exists(assets_dir)) {
    return;
  }

  for (auto& entry : fs::recursive_directory_iterator(assets_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    std::string ext = entry.path().extension().string();
    if (ext == ".meta") {
      continue;
    }

    AssetType type = ExtToAssetType(ext);
    if (type == AssetType::None) {
      continue;
    }

    auto rel = fs::relative(entry.path(), assets_dir);
    std::string vfs_path = "/app/" + rel.generic_string();
    std::string name = entry.path().stem().string();

    AssetHandle handle = ImportAsset(name, type, vfs_path);
    if (!handle.IsValid()) {
      continue;
    }

    // Type-specific post-processing

    if (type == AssetType::Material) {
      if (!mgr.Get<Material>(handle)) {
        try {
          VfsFile file = Engine::vfs()->Open(vfs_path);
          if (file) {
            std::string content((std::istreambuf_iterator<char>(file.Stream())),
                                std::istreambuf_iterator<char>());
            auto j = nlohmann::json::parse(content);
            auto mat = Material::Deserialize(j);
            mat->name = name;
            mat->asset_handle = handle;
            mgr.Store<Material>(handle, mat);
            mgr.SetLoadState(handle, AssetLoadState::Unloaded,
                             AssetLoadState::Loaded);
          }
        } catch (const std::exception& e) {
          LOG_ERROR("Failed to deserialize material '{}': {}", vfs_path,
                    e.what());
        }
      }
    }

    if (type == AssetType::Scene) {
      Engine::scene_manager().RegisterScene(name, entry.path());

      try {
        VfsFile file = Engine::vfs()->Open(vfs_path);
        if (file) {
          std::string content((std::istreambuf_iterator<char>(file.Stream())),
                              std::istreambuf_iterator<char>());
          auto j = nlohmann::json::parse(content);
          if (j.value("preload_assets", false)) {
            scenes_to_preload.push_back(vfs_path);
          }
        }
      } catch (...) {}
    }

    if (type == AssetType::Prefab || type == AssetType::Scene) {
      mgr.SetLoadState(handle, AssetLoadState::Unloaded,
                       AssetLoadState::Loaded);
    }
  }

  // Clean up orphaned .meta files
  for (auto& entry : fs::recursive_directory_iterator(assets_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() != ".meta") {
      continue;
    }
    fs::path asset_path =
        entry.path().string().substr(0, entry.path().string().size() - 5);
    if (!fs::exists(asset_path)) {
      std::error_code ec;
      fs::remove(entry.path(), ec);
    }
  }

  // Preload assets for scenes that have preload_assets enabled.
  for (const auto& preload_vfs : scenes_to_preload) {
    VfsFile file = Engine::vfs()->Open(preload_vfs);
    if (!file) {
      continue;
    }
    std::string content((std::istreambuf_iterator<char>(file.Stream())),
                        std::istreambuf_iterator<char>());
    auto temp_scene = std::make_shared<Scene>();
    SceneSerializer serializer(temp_scene);
    if (serializer.DeserializeFromString(content)) {
      LOG_INFO("Preloading assets for scene: {}", preload_vfs);
    }
  }
}

void ProjectLoader::ApplyRenderOptions(Project& project) {
  GameLoader::ApplyRenderOptions(project.GetGameInfo());
}

void ProjectLoader::ApplyInputSettings(Project& project) {
  GameLoader::ApplyInputSettings(project.GetGameInfo());
}

bool ProjectLoader::LoadStartScene(Project& project) {
  return GameLoader::LoadStartScene(project.GetGameInfo());
}

bool ProjectLoader::LoadAll(Project& project) {
  if (!MountProject(project)) {
    return false;
  }

  ScanAssets(project);
  Engine::script_manager().Reload();
  ApplyRenderOptions(project);
  ApplyInputSettings(project);
  LoadStartScene(project);

  return true;
}

}  // namespace Wiesel
