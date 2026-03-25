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
// Created by Metehan Gezer on 05.03.2026.
//

#include "scene/w_scene_manager.h"

#include "asset/w_asset_manager.h"
#include "scene/w_scene_serializer.h"
#include "util/w_logger.h"
#include "w_engine.h"

namespace Wiesel {

std::shared_ptr<Scene> SceneManager::CreateScene() {
  active_scene_ = std::make_shared<Scene>();
  return active_scene_;
}

void SceneManager::RegisterScene(const std::string& name,
                                 const std::filesystem::path& path) {
  registered_scenes_[name] = std::filesystem::absolute(path);
  LOG_INFO("Registered scene '{}' at {}", name, path.string());
}

void SceneManager::UnregisterScene(const std::string& name) {
  registered_scenes_.erase(name);
}

void SceneManager::LoadScene(const std::string& name) {
  auto it = registered_scenes_.find(name);
  if (it == registered_scenes_.end()) {
    LOG_ERROR("Scene '{}' not registered", name);
    return;
  }
  pending_scene_path_ = it->second;
  LOG_INFO("Queued scene load: '{}'", name);
}

void SceneManager::LoadSceneFromPath(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    LOG_ERROR("Scene file not found: {}", path.string());
    return;
  }
  pending_scene_path_ = std::filesystem::absolute(path);
  LOG_INFO("Queued scene load from path: {}", path.string());
}

bool SceneManager::BeginFrame() {
  auto scene = active_scene_;
  if (!scene) {
    return false;
  }

  // Poll async asset loading progress via target scene's own tracking
  if (target_scene_ && !scene_ready_) {
    load_progress_ = target_scene_->GetAssetLoadProgress();
    if (target_scene_->AreAssetsReady()) {
      scene_ready_ = true;
      target_scene_.reset();
      LOG_INFO("Target scene assets pre-loaded");
    }
  }

  // Auto-activate target scene when pre-loading is done
  if (auto_activate_ && scene_ready_ && !target_scene_path_.empty()) {
    ActivateLoadedScene();
  }

  if (pending_scene_path_.empty()) {
    return false;
  }

  std::filesystem::path path = pending_scene_path_;
  pending_scene_path_.clear();

  // Save old scene's asset list and keep-loaded flag before clearing
  std::vector<AssetHandle> old_assets = scene->GetRequestedAssets();
  bool old_keep_loaded = scene->GetKeepAssetsLoaded();

  // Wait for GPU to finish before destroying resources
  Engine::renderer()->WaitForGPU();

  // Clear the current scene
  auto& hierarchy = scene->GetSceneHierarchy();
  std::vector<entt::entity> to_remove(hierarchy.begin(), hierarchy.end());
  for (auto entity_id : to_remove) {
    Entity entity{entity_id, scene.get()};
    scene->RemoveEntity(entity);
  }
  scene->ProcessDestroyQueue();
  scene->ResetPhysicsWorld();
  scene->InvalidateRenderGraphs();
  scene->ClearRequestedAssets();

  // Load the new scene via VFS (populates requested_assets_ via RequestAsset)
  auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
  std::string vfs_path;
  if (physical_app.has_value()) {
    auto rel = std::filesystem::relative(path, *physical_app);
    vfs_path = "/app/" + rel.generic_string();
  }

  VfsFile file = Engine::vfs()->Open(vfs_path);
  if (!file) {
    LOG_ERROR("Failed to open scene: {}", vfs_path);
    return false;
  }
  std::string content((std::istreambuf_iterator<char>(file.Stream())),
                      std::istreambuf_iterator<char>());
  SceneSerializer serializer(scene);
  if (!serializer.DeserializeFromString(content)) {
    LOG_ERROR("Failed to load scene: {}", path.string());
    return false;
  }

  // Unload assets the old scene used but the new scene doesn't,
  // unless the old scene had keep_assets_loaded set
  if (!old_keep_loaded) {
    UnloadUnusedAssets(old_assets, scene->GetRequestedAssets());
  }

  // Setup cameras
  auto view = scene->GetAllEntitiesWith<CameraComponent>();
  for (auto entity : view) {
    auto& cam = scene->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  scene->ResetFirstUpdate();
  LOG_INFO("Scene loaded: {}", path.string());
  return true;
}

void SceneManager::LoadSceneWithLoading(const std::string& target_scene,
                                        const std::string& loading_scene) {
  auto target_it = registered_scenes_.find(target_scene);
  auto loading_it = registered_scenes_.find(loading_scene);
  if (target_it == registered_scenes_.end()) {
    LOG_ERROR("Target scene '{}' not registered", target_scene);
    return;
  }
  if (loading_it == registered_scenes_.end()) {
    LOG_ERROR("Loading scene '{}' not registered", loading_scene);
    return;
  }
  LoadSceneWithLoadingPath(target_it->second, loading_it->second);
}

void SceneManager::LoadSceneWithLoadingPath(
    const std::filesystem::path& target_path,
    const std::filesystem::path& loading_path) {
  if (!std::filesystem::exists(target_path)) {
    LOG_ERROR("Target scene not found: {}", target_path.string());
    return;
  }
  if (!std::filesystem::exists(loading_path)) {
    LOG_ERROR("Loading scene not found: {}", loading_path.string());
    return;
  }

  target_scene_path_ = std::filesystem::absolute(target_path);
  load_progress_ = 0.0f;
  scene_ready_ = false;
  auto_activate_ = true;

  // Switch to loading scene immediately
  pending_scene_path_ = std::filesystem::absolute(loading_path);
  LOG_INFO("Loading via intermediate scene: {} -> {}", loading_path.string(),
           target_path.string());

  // Deserialize the target scene into a temporary scene to discover and
  // kick off async loads for all asset dependencies. RequestAsset() is
  // called during deserialization for every asset handle, so we don't
  // need to manually scan the JSON for specific component types.
  target_scene_ = std::make_shared<Scene>();
  auto target_app = Engine::vfs()->GetPhysicalPath("/app");
  std::string target_vfs;
  if (target_app.has_value()) {
    auto rel = std::filesystem::relative(target_scene_path_, *target_app);
    target_vfs = "/app/" + rel.generic_string();
  }
  VfsFile target_file = Engine::vfs()->Open(target_vfs);
  std::string target_content;
  if (target_file) {
    target_content =
        std::string((std::istreambuf_iterator<char>(target_file.Stream())),
                    std::istreambuf_iterator<char>());
  }
  SceneSerializer serializer(target_scene_);
  if (target_content.empty() ||
      !serializer.DeserializeFromString(target_content)) {
    LOG_ERROR("Failed to pre-parse target scene: {}",
              target_scene_path_.string());
    target_scene_.reset();
    load_progress_ = 1.0f;
    scene_ready_ = true;
    return;
  }

  if (target_scene_->AreAssetsReady()) {
    // All assets already loaded (or no async assets)
    target_scene_.reset();
    load_progress_ = 1.0f;
    scene_ready_ = true;
    return;
  }

  LOG_INFO("Pre-loading assets for target scene (progress: {:.0f}%)",
           target_scene_->GetAssetLoadProgress() * 100.0f);
}

void SceneManager::ActivateLoadedScene() {
  if (!scene_ready_ || target_scene_path_.empty()) {
    LOG_WARN("No scene ready to activate");
    return;
  }

  pending_scene_path_ = target_scene_path_;
  target_scene_path_.clear();
  load_progress_ = 0.0f;
  scene_ready_ = false;
  auto_activate_ = false;
  LOG_INFO("Activating loaded scene");
}

void SceneManager::EndFrame() {
  if (active_scene_) {
    active_scene_->ProcessDestroyQueue();
  }
}

void SceneManager::Cleanup() {
  if (active_scene_) {
    active_scene_->Cleanup();
    active_scene_.reset();
  }
}

void SceneManager::UnloadUnusedAssets(
    const std::vector<AssetHandle>& old_assets,
    const std::vector<AssetHandle>& new_assets) {
  int unloaded = 0;
  for (const auto& old_handle : old_assets) {
    // Check if new scene still needs this asset
    bool still_needed = false;
    for (const auto& new_handle : new_assets) {
      if (old_handle == new_handle) {
        still_needed = true;
        break;
      }
    }
    if (still_needed) {
      continue;
    }

    // Only unload if actually loaded
    auto state = Engine::asset_manager().GetLoadState(old_handle);
    if (state == AssetLoadState::Loaded) {
      Engine::asset_manager().Unload(old_handle);
      unloaded++;
    }
  }
  if (unloaded > 0) {
    LOG_INFO("Unloaded {} unused assets from previous scene", unloaded);
  }
}

}  // namespace Wiesel