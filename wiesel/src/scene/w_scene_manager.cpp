//
// Created by Metehan Gezer on 05.03.2026.
//

#include "scene/w_scene_manager.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>

#include "asset/w_asset_manager.hpp"
#include "rendering/w_mesh.hpp"
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

  // Store the target for async loading
  target_scene_path_ = std::filesystem::absolute(target_path);
  load_progress_ = 0.0f;
  scene_ready_ = false;

  // Immediately switch to the loading scene
  pending_scene_path_ = std::filesystem::absolute(loading_path);
  LOG_INFO("Loading via intermediate scene: {} -> {}",
           loading_path.string(), target_path.string());

  // Pre-load target scene's assets in background
  auto target = target_scene_path_;
  std::thread([this, target]() {
    // Parse the scene JSON to find referenced assets
    std::ifstream file(target);
    if (!file.is_open()) {
      LOG_ERROR("Failed to open target scene for pre-loading: {}", target.string());
      load_progress_ = 1.0f;
      scene_ready_ = true;
      return;
    }

    try {
      nlohmann::json root;
      file >> root;
      file.close();

      if (!root.contains("entities") || !root["entities"].is_array()) {
        load_progress_ = 1.0f;
        scene_ready_ = true;
        return;
      }

      // Collect model asset handles that need loading
      std::vector<AssetHandle> models_to_load;
      for (auto& ej : root["entities"]) {
        if (ej.contains("Model")) {
          std::string handle_str = ej["Model"].value("asset_handle", "");
          if (!handle_str.empty()) {
            AssetHandle h = AssetHandle::FromString(handle_str);
            if (h.IsValid() && !Engine::asset_manager().Get<Model>(h)) {
              models_to_load.push_back(h);
            }
          }
        }
      }

      // Kick off async model loads
      int total = static_cast<int>(models_to_load.size());
      if (total == 0) {
        load_progress_ = 1.0f;
        scene_ready_ = true;
        return;
      }

      for (auto& handle : models_to_load) {
        Engine::LoadModelAsync(handle);
      }

      // Poll until all models are loaded
      while (true) {
        int loaded = 0;
        for (auto& handle : models_to_load) {
          auto state = Engine::asset_manager().GetLoadState(handle);
          if (state == AssetLoadState::Loaded || state == AssetLoadState::Failed) {
            loaded++;
          }
        }
        load_progress_ = static_cast<float>(loaded) / static_cast<float>(total);
        if (loaded >= total) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    } catch (const std::exception& e) {
      LOG_ERROR("Error pre-loading scene assets: {}", e.what());
    }

    load_progress_ = 1.0f;
    scene_ready_ = true;
    LOG_INFO("Target scene assets pre-loaded");
  }).detach();
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
  LOG_INFO("Activating loaded scene");
}

}  // namespace Wiesel