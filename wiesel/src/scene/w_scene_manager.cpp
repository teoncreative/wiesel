//
// Created by Metehan Gezer on 05.03.2026.
//

#include "scene/w_scene_manager.hpp"

#include "scene/w_scene_serializer.hpp"
#include "util/w_logger.hpp"
#include "w_engine.hpp"

namespace Wiesel {

SceneManager SceneManager::instance_;

SceneManager& SceneManager::Get() {
  return instance_;
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

bool SceneManager::ProcessPendingLoad(std::shared_ptr<Scene> scene) {
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

}  // namespace Wiesel