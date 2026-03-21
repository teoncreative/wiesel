//
// Created by Metehan Gezer on 05.03.2026.
//

#pragma once

#include "scene/w_scene.hpp"
#include "w_pch.hpp"

namespace Wiesel {

class SceneManager {
 public:
  SceneManager() = default;

  // Scene ownership - SceneManager is the single authority
  std::shared_ptr<Scene> CreateScene();
  void SetActiveScene(std::shared_ptr<Scene> scene) { active_scene_ = scene; }
  std::shared_ptr<Scene> GetActiveScene() const { return active_scene_; }

  // Register a scene file path with a name (e.g. "MainMenu", "Level1")
  void RegisterScene(const std::string& name,
                     const std::filesystem::path& path);
  void UnregisterScene(const std::string& name);
  void ClearRegisteredScenes() { registered_scenes_.clear(); }

  // Get all registered scenes
  const std::map<std::string, std::filesystem::path>& GetRegisteredScenes()
      const {
    return registered_scenes_;
  }

  // Scene switching - queues a scene load for the next frame
  // This is safe to call from scripts during update
  void LoadScene(const std::string& name);
  void LoadSceneFromPath(const std::filesystem::path& path);

  // Load with an intermediate loading screen scene
  // 1. Immediately loads loading_scene
  // 2. Begins async-loading target_scene in background
  // 3. Loading screen scripts can query progress
  // 4. When ready, call ActivateLoadedScene() to switch
  void LoadSceneWithLoading(const std::string& target_scene,
                             const std::string& loading_scene);
  void LoadSceneWithLoadingPath(const std::filesystem::path& target_path,
                                 const std::filesystem::path& loading_path);

  // Query async loading state (for loading screen scripts)
  float GetLoadProgress() const { return load_progress_; }
  bool IsSceneReady() const { return scene_ready_; }

  // Switch to the async-loaded scene (call from loading screen when ready)
  void ActivateLoadedScene();

  // Check if a scene switch is pending
  bool HasPendingSceneLoad() const { return !pending_scene_path_.empty(); }
  const std::filesystem::path& GetPendingScenePath() const {
    return pending_scene_path_;
  }

  // Per-frame lifecycle - layers call these
  // BeginFrame: processes pending scene loads. Returns true if a scene was switched.
  bool BeginFrame();
  // EndFrame: processes entity destroy queue
  void EndFrame();
  // Cleanup: called on shutdown, cleans up the active scene
  void Cleanup();

  // Clear pending load (e.g. if editor cancels)
  void ClearPending() {
    pending_scene_path_.clear();
    target_scene_path_.clear();
    target_scene_.reset();
    scene_ready_ = false;
    load_progress_ = 0.0f;
    auto_activate_ = false;
  }

 private:
  std::shared_ptr<Scene> active_scene_;
  std::map<std::string, std::filesystem::path> registered_scenes_;
  std::filesystem::path pending_scene_path_;

  // Async loading state
  std::filesystem::path target_scene_path_;
  std::shared_ptr<Scene> target_scene_;
  float load_progress_ = 0.0f;
  bool scene_ready_ = false;
  bool auto_activate_ = false;
};

}  // namespace Wiesel