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

#pragma once

#include "scene/w_entity.h"
#include "scene/w_scene.h"
#include "w_pch.h"

namespace Wiesel {

enum class LoadSceneMode { Single, Additive };

class SceneManager {
 public:
  SceneManager() = default;

  // Scene ownership - SceneManager is the single authority
  std::shared_ptr<Scene> CreateScene();

  void SetActiveScene(std::shared_ptr<Scene> scene) { active_scene_ = scene; }

  std::shared_ptr<Scene> GetActiveScene() const { return active_scene_; }

  // All currently loaded scenes (includes active scene)
  const std::vector<std::shared_ptr<Scene>>& GetLoadedScenes() const {
    return loaded_scenes_;
  }

  MultiScene& GetMultiScene() { return multi_scene_; }

  // Find a loaded scene by name or raw pointer
  std::shared_ptr<Scene> FindScene(const std::string& name) const;
  std::shared_ptr<Scene> FindSceneByPtr(Scene* raw) const;

  // Register a scene file path with a name (e.g. "MainMenu", "Level1")
  void RegisterScene(const std::string& name, const std::string& vfs_path);
  void UnregisterScene(const std::string& name);

  void ClearRegisteredScenes() { registered_scenes_.clear(); }

  // Get all registered scenes
  const std::map<std::string, std::string>& GetRegisteredScenes() const {
    return registered_scenes_;
  }

  // Synchronous scene loading - loads immediately and returns the scene.
  // Single: replaces all loaded scenes with the new one.
  // Additive: loads on top of existing scenes.
  std::shared_ptr<Scene> LoadScene(const std::string& name,
                                   LoadSceneMode mode = LoadSceneMode::Single);
  std::shared_ptr<Scene> LoadSceneFromPath(
      const std::string& vfs_path, LoadSceneMode mode = LoadSceneMode::Single);

  // Async scene loading - queued for next BeginFrame.
  // Safe to call from scripts during update.
  void LoadSceneAsync(const std::string& name,
                      LoadSceneMode mode = LoadSceneMode::Single);
  void LoadSceneAsyncFromPath(const std::string& vfs_path,
                              LoadSceneMode mode = LoadSceneMode::Single);

  // Unload a specific additively-loaded scene (queued for end of frame)
  void UnloadScene(std::shared_ptr<Scene> scene);
  void UnloadScene(const std::string& name);

  // Immediately unload all additively-loaded scenes (keeps only the active scene)
  void UnloadAllAdditiveScenes();

  // Move an entity (and optionally its children) from one scene to another.
  // Returns the new Entity in the target scene (old entity handle is invalidated).
  Entity MoveEntityToScene(Entity entity, std::shared_ptr<Scene> target_scene,
                           bool move_children = true);

  // Load with an intermediate loading screen scene
  // 1. Immediately loads loading_scene
  // 2. Begins async-loading target_scene in background
  // 3. Loading screen scripts can query progress
  // 4. When ready, call ActivateLoadedScene() to switch
  void LoadSceneWithLoading(const std::string& target_scene,
                            const std::string& loading_scene);

  // Query async loading state (for loading screen scripts)
  float GetLoadProgress() const { return load_progress_; }

  bool IsSceneReady() const { return scene_ready_; }

  // Switch to the async-loaded scene (call from loading screen when ready)
  void ActivateLoadedScene();

  // Check if any async scene loads are pending
  bool HasPendingSceneLoad() const { return !pending_async_loads_.empty(); }

  // Rendering - centralized render methods that handle all loaded scenes.
  // Each camera sees entities from ALL loaded scenes.
  bool RenderGameView();
  bool RenderEditorView(CameraComponent& camera, TransformComponent& transform,
                        bool show_grid = false);

  // Per-frame lifecycle - layers call these
  // BeginFrame: processes pending scene loads. Returns true if a scene was switched.
  bool BeginFrame();
  // EndFrame: processes entity destroy queue
  void EndFrame();
  // Cleanup: called on shutdown, cleans up the active scene
  void Cleanup();

  // Clear pending load (e.g. if editor cancels)
  void ClearPending() {
    pending_async_loads_.clear();
    pending_unloads_.clear();
    target_scene_path_.clear();
    target_scene_.reset();
    scene_ready_ = false;
    load_progress_ = 0.0f;
    auto_activate_ = false;
  }

  // Unload assets that were used by the old scene but not the new one.
  void UnloadUnusedAssets(const std::vector<AssetHandle>& old_assets,
                          const std::vector<AssetHandle>& new_assets);

  void OnUpdate(float_t delta_time);
  void OnUpdateEditor(float_t delta_time);

  std::shared_ptr<RenderPipeline> GetDefaultPipeline() {
    return default_pipeline_;
  }

  uint32_t GetPipelineVersion() const { return pipeline_version_; }

  std::shared_ptr<RenderGraph> GetExternalRenderGraph() const {
    return external_render_graph_;
  }

 private:
  void ProcessPendingAsyncLoads();
  void ProcessPendingUnloads();
  void AccumulateLights();
  void EnsureDefaultResources();
  void CreateDefaultPipeline();

  // Internal: load a scene from VFS path and add to loaded_scenes_.
  // Does NOT replace the active scene - caller handles that for Single mode.
  std::shared_ptr<Scene> LoadAdditiveFromPath(const std::string& vfs_path,
                                              const std::string& name);
  // Internal: replace the active scene contents from VFS path.
  bool ReplacePrimaryScene(const std::string& vfs_path);

  static std::string DeriveNameFromPath(const std::string& vfs_path);
  // Unload assets from a removed scene that no other loaded scene needs.
  void UnloadUnusedAssetsForScene(const std::vector<AssetHandle>& scene_assets);

  std::shared_ptr<Scene> active_scene_;
  std::vector<std::shared_ptr<Scene>> loaded_scenes_;
  MultiScene multi_scene_{loaded_scenes_};
  std::map<std::string, std::string> registered_scenes_;
  // Async loading state
  struct PendingAsyncLoad {
    std::string vfs_path;
    std::string name;
    LoadSceneMode mode;
  };

  std::vector<PendingAsyncLoad> pending_async_loads_;
  std::vector<std::shared_ptr<Scene>> pending_unloads_;

  // Async loading state
  std::string target_scene_path_;
  std::shared_ptr<Scene> target_scene_;
  float load_progress_ = 0.0f;
  bool scene_ready_ = false;
  bool auto_activate_ = false;

  // Rendering state
  std::shared_ptr<RenderPipeline> default_pipeline_;
  std::shared_ptr<RenderGraph> external_render_graph_;
  // Monotonically increasing version. Cameras compare their
  // resource_pipeline_version against this to detect staleness.
  uint32_t pipeline_version_ = 0;
};

}  // namespace Wiesel