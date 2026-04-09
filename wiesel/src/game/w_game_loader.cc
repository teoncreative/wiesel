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
#include "asset/w_asset_registry.h"
#include "asset/w_asset_utils.h"
#include "input/w_input.h"
#include "rendering/w_renderer.h"
#include "scene/w_scene_manager.h"
#include "scene/w_scene_serializer.h"
#include "script/w_scriptmanager.h"
#include "w_engine.h"

namespace Wiesel {

AssetHandle GameLoader::ImportAsset(const std::string& name, AssetType type,
                                    const std::string& vfs_path) {
  AssetManager& mgr = Engine::asset_manager();
  AssetHandle handle;

  auto physical = Engine::vfs()->GetPhysicalPath(vfs_path);

  // All asset types use .meta files for lightweight registration.
  // Legacy JSON assets that embed asset_handle are migrated on first scan.
  AssetRegistry::MetaFileData meta_data;
  std::filesystem::path meta_path;
  if (physical.has_value()) {
    meta_path = physical->string() + ".meta";
    meta_data = AssetRegistry::ReadMetaFile(meta_path);
  }

  if (meta_data.handle.IsValid()) {
    handle = meta_data.handle;
    if (!mgr.HasAsset(handle)) {
      mgr.Register(handle, name, type, vfs_path);
    }
  } else if (AssetRegistry::HasSerializer(type)) {
    // Migration: read handle from legacy JSON asset and create .meta
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
      LOG_ERROR("Failed to import '{}': {}", vfs_path, e.what());
      return {};
    }

    if (!handle.IsValid()) {
      handle = AssetHandle::Generate();
    }

    if (!mgr.HasAsset(handle)) {
      mgr.Register(handle, name, type, vfs_path);
    }

    if (physical.has_value()) {
      AssetRegistry::WriteMetaFile(meta_path, handle, type);
    }
  } else if (!physical.has_value()) {
    return {};
  } else {
    handle = mgr.Register(name, type, vfs_path);
    if (handle.IsValid()) {
      AssetRegistry::WriteMetaFile(meta_path, handle, type);
    }
  }

  if (handle.IsValid()) {
    auto* metadata = const_cast<AssetMetadata*>(mgr.GetMetadata(handle));
    if (metadata) {
      const auto* desc = AssetRegistry::Get(type);
      if (desc && desc->HasProperties()) {
        if (!meta_data.properties.empty()) {
          metadata->properties =
              desc->DeserializeProperties(meta_data.properties);
        } else {
          metadata->properties = desc->CreateProperties();
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

  return handle;
}

bool GameLoader::MountAssets(const std::filesystem::path& assets_dir) {
  namespace fs = std::filesystem;
  auto* vfs = Engine::vfs().get();
  if (fs::exists(assets_dir)) {
    vfs->Unmount("app://");
    vfs->Mount("app://", fs::absolute(assets_dir).string());
    return true;
  }
  return false;
}

void GameLoader::ScanVfsPrefix(const std::string& prefix,
                               RegisterFn register_fn,
                               std::vector<std::string>& scenes_to_preload) {
  namespace fs = std::filesystem;
  AssetManager& mgr = Engine::asset_manager();

  VirtualFileSystem* vfs = Engine::vfs().get();
  std::vector<std::string> all_files = vfs->ListFiles(prefix, true);

  for (const std::string& vfs_path : all_files) {
    fs::path rel_path(vfs_path.substr(prefix.size()));
    std::string ext = rel_path.extension().string();
    if (ext == ".meta") {
      continue;
    }

    AssetType type = ExtToAssetType(ext);
    if (type == AssetType::None) {
      continue;
    }

    std::string name = rel_path.stem().string();

    AssetHandle handle = register_fn(name, type, vfs_path);
    if (!handle.IsValid()) {
      continue;
    }

    // Type-specific post-processing

    if (type == AssetType::Material) {
      if (!mgr.IsLoaded(handle)) {
        mgr.LoadSync(handle);
      }
    }

    if (type == AssetType::Scene) {
      Engine::scene_manager().RegisterScene(name, vfs_path);

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
      } catch (const std::exception& e) {
        LOG_ERROR("Failed to read scene '{}': {}", vfs_path, e.what());
      }
    }

    if (type == AssetType::Prefab || type == AssetType::Scene) {
      mgr.SetLoadState(handle, AssetLoadState::Unloaded,
                       AssetLoadState::Loaded);
    }
  }
}

void GameLoader::ScanAssets() {
  Engine::scene_manager().ClearRegisteredScenes();
  std::vector<std::string> scenes_to_preload;

  // Scan engine assets first, then project assets.
  // ImportAsset creates .meta files when physical storage is available,
  // RegisterAsset is read-only (e.g. from pak files).
  ScanVfsPrefix("engine://", ImportAsset, scenes_to_preload);
  ScanVfsPrefix("app://", ImportAsset, scenes_to_preload);

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
  settings.shadow_map_resolution = opts.shadow_resolution;
  settings.anisotropic_filtering = opts.anisotropic_filtering;
  settings.texture_quality = opts.texture_quality;
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
  out_opts.shadow_resolution = settings.shadow_map_resolution;
  out_opts.anisotropic_filtering = settings.anisotropic_filtering;
  out_opts.texture_quality = settings.texture_quality;
}

void GameLoader::ApplyInputSettings(const GameInfo& info) {
  if (!info.input.contexts.empty()) {
    Engine::input().LoadFromSettings(info.input);
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
