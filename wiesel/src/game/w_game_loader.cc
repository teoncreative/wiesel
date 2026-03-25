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
// Created by Metehan Gezer on 24.03.2026.
//

#include "game/w_game_loader.h"

#include <stb_image.h>
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

GameLoader::MetaFileData GameLoader::ReadMetaFile(
    const std::filesystem::path& meta_path) {
  MetaFileData result;
  if (!std::filesystem::exists(meta_path)) {
    return result;
  }
  std::ifstream file(meta_path);
  if (!file.is_open()) {
    return result;
  }
  try {
    nlohmann::json j;
    file >> j;
    result.handle = AssetHandle::FromString(j.value("handle", ""));
    if (j.contains("properties") && j["properties"].is_object()) {
      result.properties = j["properties"];
    }
  } catch (...) {}
  return result;
}

bool GameLoader::MountAssets(const std::filesystem::path& assets_dir) {
  namespace fs = std::filesystem;
  auto* vfs = Engine::vfs().get();
  if (fs::exists(assets_dir)) {
    vfs->Unmount("/app");
    vfs->Mount("/app", fs::absolute(assets_dir).string());
    return true;
  }
  return false;
}

// Read-only: register a single asset. Returns the handle if the asset
// already has one (from .meta or embedded JSON). Never generates new handles.
static AssetHandle RegisterAsset(const std::string& name, AssetType type,
                                 const std::string& vfs_path) {
  AssetManager& mgr = Engine::asset_manager();
  AssetHandle handle;

  auto physical = Engine::vfs()->GetPhysicalPath(vfs_path);

  if (IsJsonAssetType(type)) {
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
    } catch (const std::exception& e) {
      LOG_ERROR("Failed to read '{}': {}", vfs_path, e.what());
      return {};
    }

    if (!handle.IsValid()) {
      LOG_WARN("Asset '{}' has no handle, skipping", vfs_path);
      return {};
    }

    if (!mgr.HasAsset(handle)) {
      mgr.Register(handle, name, type, vfs_path);
    }
  } else {
    // Binary assets: read .meta sidecar
    GameLoader::MetaFileData meta_data;
    if (physical.has_value()) {
      std::filesystem::path meta_path = physical->string() + ".meta";
      meta_data = GameLoader::ReadMetaFile(meta_path);
    }

    if (!meta_data.handle.IsValid()) {
      LOG_WARN("Asset '{}' has no .meta handle, skipping", vfs_path);
      return {};
    }

    handle = meta_data.handle;
    if (!mgr.HasAsset(handle)) {
      mgr.Register(handle, name, type, vfs_path);
    }

    // Set up asset properties from .meta
    if (handle.IsValid()) {
      auto* metadata = const_cast<AssetMetadata*>(mgr.GetMetadata(handle));
      if (metadata) {
        const auto* desc = AssetPropertyRegistry::Get(type);
        if (desc) {
          if (!meta_data.properties.empty()) {
            metadata->properties = desc->Deserialize(meta_data.properties);
          } else {
            metadata->properties = desc->Create();
          }
        }
      }
    }
  }

  return handle;
}

void GameLoader::ScanAssets() {
  namespace fs = std::filesystem;
  AssetManager& mgr = Engine::asset_manager();
  Engine::scene_manager().ClearRegisteredScenes();
  std::vector<std::string> scenes_to_preload;

  auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
  if (!physical_app.has_value()) {
    return;
  }
  fs::path assets_dir = fs::absolute(*physical_app);
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

    AssetHandle handle = RegisterAsset(name, type, vfs_path);
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

void GameLoader::ApplyRenderOptions(const GameInfo& info) {
  auto& opts = info.render_options;
  auto& settings = Engine::renderer()->options();
  settings.ambient_color = opts.ambient_color;
  settings.ambient_intensity = opts.ambient_intensity;
  settings.ssao_enabled = opts.ssao_enabled;
  settings.ibl_enabled = opts.ibl_enabled;
  settings.bloom_enabled = opts.bloom_enabled;
  settings.bloom_threshold = opts.bloom_threshold;
  settings.bloom_intensity = opts.bloom_intensity;
  settings.motion_blur_enabled = opts.motion_blur_enabled;
  settings.motion_blur_strength = opts.motion_blur_strength;
  settings.motion_blur_samples = opts.motion_blur_samples;
  settings.shadows_enabled = opts.shadows_enabled;
  settings.vsync = opts.vsync;
  settings.aa_mode = static_cast<AntiAliasingMode>(opts.aa_mode);
  settings.msaa_mode = static_cast<SamplingMode>(opts.msaa_mode);
}

void GameLoader::CaptureRenderOptions(RenderOptionsSerialized& out_opts) {
  auto& settings = Engine::renderer()->options();
  out_opts.ambient_color = settings.ambient_color;
  out_opts.ambient_intensity = settings.ambient_intensity;
  out_opts.ssao_enabled = settings.ssao_enabled;
  out_opts.ibl_enabled = settings.ibl_enabled;
  out_opts.bloom_enabled = settings.bloom_enabled;
  out_opts.bloom_threshold = settings.bloom_threshold;
  out_opts.bloom_intensity = settings.bloom_intensity;
  out_opts.motion_blur_enabled = settings.motion_blur_enabled;
  out_opts.motion_blur_strength = settings.motion_blur_strength;
  out_opts.motion_blur_samples = settings.motion_blur_samples;
  out_opts.shadows_enabled = settings.shadows_enabled;
  out_opts.vsync = settings.vsync;
  out_opts.aa_mode =
      static_cast<int>(static_cast<AntiAliasingMode>(settings.aa_mode));
  out_opts.msaa_mode =
      static_cast<int>(static_cast<SamplingMode>(settings.msaa_mode));
}

void GameLoader::ApplyInputSettings(const GameInfo& info) {
  if (!info.input.contexts.empty()) {
    InputManager::LoadFromSettings(info.input);
  }
}

bool GameLoader::LoadStartScene(const GameInfo& info) {
  if (!info.start_scene.IsValid()) {
    return false;
  }

  const auto* meta = Engine::asset_manager().GetMetadata(info.start_scene);
  if (!meta) {
    return false;
  }

  VfsFile file = Engine::vfs()->Open(meta->virtual_source_path);
  if (!file) {
    return false;
  }

  auto scene = Engine::scene_manager().GetActiveScene();
  if (!scene) {
    return false;
  }

  std::string content((std::istreambuf_iterator<char>(file.Stream())),
                      std::istreambuf_iterator<char>());
  SceneSerializer serializer(scene);
  if (!serializer.DeserializeFromString(content)) {
    return false;
  }

  for (auto entity : scene->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = scene->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  LOG_INFO("Loaded start scene: {}", meta->virtual_source_path);
  return true;
}

bool GameLoader::LoadAll(const GameInfo& info,
                         const std::filesystem::path& assets_dir) {
  if (!MountAssets(assets_dir)) {
    return false;
  }

  ScanAssets();
  Engine::script_manager().Reload();
  // Set window title from game name
  if (!info.name.empty()) {
    Engine::window()->SetTitle(info.name);
  }

  // Set window icon
  if (info.icon.IsValid()) {
    const auto* meta = Engine::asset_manager().GetMetadata(info.icon);
    if (meta) {
      auto file = Engine::vfs()->Open(meta->virtual_source_path);
      if (file) {
        int w = 0;
        int h = 0;
        int channels = 0;
        stbi_uc* pixels =
            stbi_load_from_memory(file.Data(), static_cast<int>(file.Size()),
                                  &w, &h, &channels, STBI_rgb_alpha);
        if (pixels) {
          Engine::window()->SetIcon(pixels, w, h);
          stbi_image_free(pixels);
        }
      }
    }
  }

  ApplyRenderOptions(info);
  ApplyInputSettings(info);
  LoadStartScene(info);

  return true;
}

}  // namespace Wiesel
