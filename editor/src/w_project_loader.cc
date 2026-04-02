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
#include "asset/w_asset_property_registry.h"
#include "asset/w_asset_utils.h"
#include "input/w_input.h"
#include "rendering/w_material.h"
#include "rendering/w_renderer.h"
#include "scene/w_scene_manager.h"
#include "scene/w_scene_serializer.h"
#include "script/w_scriptmanager.h"
#include "w_engine.h"

namespace Wiesel {

bool ProjectLoader::MountProject(Project& project) {
  return GameLoader::MountAssets(project.GetAssetsDirectory());
}

void ProjectLoader::ScanAssets(Project& project) {
  namespace fs = std::filesystem;
  Engine::scene_manager().ClearRegisteredScenes();
  std::vector<std::string> scenes_to_preload;

  // Scan engine assets, then project assets.
  // Both use ImportAsset which creates .meta files if missing.
  GameLoader::ScanVfsPrefix("engine://", GameLoader::ImportAsset,
                            scenes_to_preload);
  GameLoader::ScanVfsPrefix("app://", GameLoader::ImportAsset,
                            scenes_to_preload);

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

}  // namespace Wiesel
