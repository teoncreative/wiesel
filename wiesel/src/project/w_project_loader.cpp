
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "project/w_project_loader.hpp"

#include <nlohmann/json.hpp>
#include "asset/w_asset_properties.hpp"

#include "asset/w_asset_manager.hpp"
#include "asset/w_asset_property_registry.hpp"
#include "input/w_input.hpp"
#include "rendering/w_material.hpp"
#include "rendering/w_renderer.hpp"
#include "scene/w_scene_manager.hpp"
#include "scene/w_scene_serializer.hpp"
#include "script/w_scriptmanager.hpp"
#include "w_engine.hpp"

namespace Wiesel {

AssetType ProjectLoader::ExtToAssetType(const std::string& ext) {
  if (ext == ".wscene") return AssetType::Scene;
  if (ext == ".wprefab") return AssetType::Prefab;
  if (ext == ".wmat") return AssetType::Material;
  if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") return AssetType::Model;
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") return AssetType::Texture;
  if (ext == ".ttf" || ext == ".otf") return AssetType::Font;
  if (ext == ".cs") return AssetType::Script;
  if (ext == ".wskybox") return AssetType::Skybox;
  if (ext == ".wspritesheet") return AssetType::SpriteSheet;
  if (ext == ".wspriteanim") return AssetType::SpriteAnim;
  if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") return AssetType::Audio;
  return AssetType::None;
}

ProjectLoader::MetaFileData ProjectLoader::ReadMetaFile(const std::filesystem::path& meta_path) {
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

void ProjectLoader::WriteMetaFile(const std::filesystem::path& meta_path,
                                  const AssetHandle& handle,
                                  AssetType type,
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

bool ProjectLoader::MountProject(Project& project) {
  namespace fs = std::filesystem;
  auto* vfs = Engine::vfs().get();
  fs::path assets_dir = project.GetAssetsDirectory();
  if (fs::exists(assets_dir)) {
    vfs->Unmount("/app");
    vfs->Mount("/app", fs::absolute(assets_dir).string());
    return true;
  }
  return false;
}

void ProjectLoader::ScanAssets(Project& project) {
  namespace fs = std::filesystem;
  AssetManager& mgr = Engine::asset_manager();
  Engine::scene_manager().ClearRegisteredScenes();
  std::vector<fs::path> scenes_to_preload;

  fs::path assets_dir = fs::absolute(project.GetAssetsDirectory());
  if (!fs::exists(assets_dir)) return;

  for (auto& entry : fs::recursive_directory_iterator(assets_dir)) {
    if (!entry.is_regular_file()) continue;

    std::string ext = entry.path().extension().string();
    if (ext == ".meta") continue;

    AssetType type = ExtToAssetType(ext);
    if (type == AssetType::None) continue;

    auto rel = fs::relative(entry.path(), assets_dir);
    std::string vfs_path = "/app/" + rel.generic_string();
    std::string name = entry.path().stem().string();

    // Types that store their handle inside their own JSON (no .meta needed)
    bool is_json_asset = (type == AssetType::Scene || type == AssetType::Prefab
                          || type == AssetType::Material || type == AssetType::Skybox
                          || type == AssetType::SpriteSheet || type == AssetType::SpriteAnim);
    AssetHandle handle;

    if (is_json_asset) {
      // Read handle from inside the JSON file, or generate + write back
      VfsFile file = Engine::vfs()->Open(vfs_path);
      if (file.Size() > 0) {
        try {
          std::string content((std::istreambuf_iterator<char>(file.Stream())),
                              std::istreambuf_iterator<char>());
          auto j = nlohmann::json::parse(content);

          std::string handle_str = j.value("asset_handle", "");
          if (!handle_str.empty()) {
            handle = AssetHandle::FromString(handle_str);
          }

          if (!handle.IsValid()) {
            // Generate a handle and write it back into the file
            handle = AssetHandle::Generate();
            j["asset_handle"] = handle.ToString();
            std::ofstream out(entry.path());
            if (out.is_open()) {
              out << j.dump(2);
            }
          }

          if (type == AssetType::Material) {
            auto mat = Material::Deserialize(j);
            mat->name = name;
            mgr.Register(handle, name, AssetType::Material, vfs_path);
            mgr.Store<Material>(handle, mat);
            mgr.SetLoadState(handle, AssetLoadState::Unloaded, AssetLoadState::Loaded);
            mat->asset_handle = handle;
          } else {
            mgr.Register(handle, name, type, vfs_path);
          }

          if (type == AssetType::Scene && j.value("preload_assets", false)) {
            scenes_to_preload.push_back(entry.path());
          }
        } catch (const std::exception& e) {
          LOG_ERROR("Failed to load '{}': {}", vfs_path, e.what());
          continue;
        }
      }
    } else {
      // Models, textures, fonts, scripts: use .meta for stable handles
      fs::path meta_path = entry.path().string() + ".meta";
      MetaFileData meta_data = ReadMetaFile(meta_path);

      if (meta_data.handle.IsValid()) {
        handle = meta_data.handle;
        if (!mgr.HasAsset(handle)) {
          mgr.Register(handle, name, type, vfs_path);
        }
      } else {
        handle = mgr.Register(name, type, vfs_path);
        if (handle.IsValid()) {
          WriteMetaFile(meta_path, handle, type);
        }
      }

      // Populate asset properties from meta file
      if (handle.IsValid()) {
        auto* metadata = const_cast<AssetMetadata*>(mgr.GetMetadata(handle));
        if (metadata) {
          const auto* desc = AssetPropertyRegistry::Get(type);
          if (desc) {
            if (!meta_data.properties.empty()) {
              metadata->properties = desc->Deserialize(meta_data.properties);
            } else {
              metadata->properties = desc->Create();
              // Auto-detect texture type from filename on first import
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

    if (!handle.IsValid()) continue;

    if (type == AssetType::Scene) {
      Engine::scene_manager().RegisterScene(name, entry.path());
      project.AddScene(rel.generic_string());
    }

    if (type == AssetType::Prefab || type == AssetType::Scene) {
      mgr.SetLoadState(handle, AssetLoadState::Unloaded, AssetLoadState::Loaded);
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
    fs::path asset_path = entry.path().string().substr(0, entry.path().string().size() - 5);
    if (!fs::exists(asset_path)) {
      std::error_code ec;
      fs::remove(entry.path(), ec);
    }
  }

  // Preload assets for scenes that have preload_assets enabled.
  // Deserialize each scene into a temporary Scene to discover dependencies
  // via RequestAsset, which kicks off async loads on the thread pool.
  for (const auto& scene_path : scenes_to_preload) {
    auto temp_scene = std::make_shared<Scene>();
    SceneSerializer serializer(temp_scene);
    if (serializer.Deserialize(scene_path)) {
      LOG_INFO("Preloading assets for scene: {}", scene_path.filename().string());
    }
    // temp_scene is discarded but async loads are already in flight
  }
}

void ProjectLoader::ApplyRenderOptions(Project& project) {
  auto& opts = project.GetSettings().render_options;
  auto& settings = Engine::renderer()->options();
  settings.ssao_enabled = opts.ssao_enabled;
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

void ProjectLoader::ApplyInputSettings(Project& project) {
  auto& input = project.GetSettings().input;
  if (!input.contexts.empty()) {
    InputManager::LoadFromSettings(input);
  }
}

bool ProjectLoader::LoadStartScene(Project& project) {
  namespace fs = std::filesystem;
  const auto& start = project.GetSettings().start_scene;
  if (start.empty()) return false;

  auto scene_path = project.GetAssetsDirectory() / start;
  if (!fs::exists(scene_path)) return false;

  auto scene = Engine::scene_manager().GetActiveScene();
  if (!scene) return false;

  SceneSerializer serializer(scene);
  if (!serializer.Deserialize(scene_path)) return false;

  for (auto entity : scene->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = scene->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  LOG_INFO("Loaded start scene: {}", scene_path.string());
  return true;
}

bool ProjectLoader::LoadAll(Project& project) {
  if (!MountProject(project)) return false;

  ScanAssets(project);
  Engine::script_manager().Reload();
  ApplyRenderOptions(project);
  ApplyInputSettings(project);
  LoadStartScene(project);

  return true;
}

}  // namespace Wiesel