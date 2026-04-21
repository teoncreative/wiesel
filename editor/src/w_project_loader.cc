//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_project_loader.h"

#include <nlohmann/json.hpp>

#include "asset/w_asset_manager.h"
#include "asset/w_asset_properties.h"
#include "asset/w_asset_registry.h"
#include "asset/w_asset_utils.h"
#include "input/w_input.h"
#include "rendering/w_material.h"
#include "rendering/w_renderer.h"
#include "scene/w_scene_manager.h"
#include "scene/w_scene_serializer.h"
#include "script/w_scriptmanager.h"
#include "w_engine.h"

namespace wiesel {

bool ProjectLoader::MountProject(Project& project) {
  return GameLoader::MountAssets(project.GetAssetsDirectory());
}

void ProjectLoader::ScanAssets(Project& project) {
  GameLoader::ScanAssets();

  namespace fs = std::filesystem;
  fs::path assets_dir = fs::absolute(project.GetAssetsDirectory());

  // Clean up orphaned .meta files (project assets only)
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

  UnregisterMissingAppAssets();
}

int ProjectLoader::UnregisterMissingAppAssets() {
  VirtualFileSystem* vfs = Engine::vfs().get();
  AssetManager& mgr = Engine::asset_manager();

  static constexpr std::string_view kAppPrefix = "app://";
  std::vector<AssetHandle> to_remove;
  for (AssetHandle handle : mgr.GetAll()) {
    const AssetMetadata* meta = mgr.GetMetadata(handle);
    if (!meta) {
      continue;
    }
    const std::string& path = meta->virtual_source_path;
    if (path.size() < kAppPrefix.size() ||
        std::string_view(path).substr(0, kAppPrefix.size()) != kAppPrefix) {
      continue;
    }
    if (!vfs->FileExists(path)) {
      to_remove.push_back(handle);
    }
  }
  for (AssetHandle handle : to_remove) {
    mgr.Unload(handle);
    mgr.Unregister(handle);
  }
  return static_cast<int>(to_remove.size());
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

bool ProjectLoader::LoadAll(Project& project, bool load_start_scene) {
  if (!MountProject(project)) {
    return false;
  }

  ScanAssets(project);
  Engine::script_manager().ReloadAsync();
  ApplyRenderOptions(project);
  ApplyInputSettings(project);

  if (load_start_scene) {
    LoadStartScene(project);
  }

  return true;
}

}  // namespace wiesel
