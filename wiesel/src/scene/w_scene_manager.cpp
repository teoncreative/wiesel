//
// Created by Metehan Gezer on 05.03.2026.
//

#include "scene/w_scene_manager.hpp"

#include "scene/w_scene_serializer.hpp"
#include "util/w_logger.hpp"
#include "w_engine.hpp"

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

  // Load the new scene
  SceneSerializer serializer(scene);
  if (!serializer.Deserialize(path)) {
    LOG_ERROR("Failed to load scene: {}", path.string());
    return false;
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

void SceneManager::LoadSceneWithLoadingPath(const std::filesystem::path& target_path,
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
  LOG_INFO("Loading via intermediate scene: {} -> {}",
           loading_path.string(), target_path.string());

  // Deserialize the target scene into a temporary scene to discover and
  // kick off async loads for all asset dependencies. RequestAsset() is
  // called during deserialization for every asset handle, so we don't
  // need to manually scan the JSON for specific component types.
  target_scene_ = std::make_shared<Scene>();
  SceneSerializer serializer(target_scene_);
  if (!serializer.Deserialize(target_scene_path_)) {
    LOG_ERROR("Failed to pre-parse target scene: {}", target_scene_path_.string());
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

}  // namespace Wiesel