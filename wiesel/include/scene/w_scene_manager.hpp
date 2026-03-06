//
// Created by Claude on 05.03.2026.
//

#pragma once

#include "scene/w_scene.hpp"
#include "w_pch.hpp"

namespace Wiesel {

class SceneManager {
 public:
  static SceneManager& Get();

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

  // Check if a scene switch is pending
  bool HasPendingSceneLoad() const { return !pending_scene_path_.empty(); }
  const std::filesystem::path& GetPendingScenePath() const {
    return pending_scene_path_;
  }

  // Called by the game loop to process pending scene loads
  // Returns true if a scene was loaded
  bool ProcessPendingLoad(Ref<Scene> scene);

  // Clear pending load (e.g. if editor cancels)
  void ClearPending() { pending_scene_path_.clear(); }

  // Active scene tracking for the editor
  void SetActiveScene(Ref<Scene> scene) { active_scene_ = scene; }
  Ref<Scene> GetActiveScene() const { return active_scene_.lock(); }

 private:
  SceneManager() = default;

  static SceneManager instance_;

  std::map<std::string, std::filesystem::path> registered_scenes_;
  std::filesystem::path pending_scene_path_;
  std::weak_ptr<Scene> active_scene_;
};

}  // namespace Wiesel