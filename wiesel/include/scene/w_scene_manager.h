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
#include "scene/w_scene_handle.h"
#include "w_pch.h"

namespace wiesel {

enum class LoadSceneMode { Single, Additive };

inline uint8_t ToInt(LoadSceneMode mode) {
  switch (mode) {
    case LoadSceneMode::Single:
      return 0;
    case LoadSceneMode::Additive:
      return 1;
  }
  return 0;
}

inline LoadSceneMode FromInt(int index) {
  switch (index) {
    case 0: return LoadSceneMode::Single;
    case 1: return LoadSceneMode::Additive;
  }
  return LoadSceneMode::Single;
}

inline std::string ToString(const LoadSceneMode& mode) {
  switch (mode) {
    case LoadSceneMode::Single:
      return "Single";
    case LoadSceneMode::Additive:
      return "Additive";
  }
  return "Invalid";
}

class SceneManager {
 public:
  SceneManager() = default;
  ~SceneManager();

  // Resolve a SceneHandle to a Scene pointer.
  // Returns nullptr if the scene was destroyed.
  Scene* Get(SceneHandle handle) const;

  // Scene ownership - SceneManager is the sole owner
  Scene* CreateScene();

  WIESEL_GETTER_FN SceneHandle GetActiveSceneHandle() const {
    return active_scene_ ? active_scene_->GetHandle() : SceneHandle{};
  }

  Scene* GetActiveScene() const { return active_scene_; }

  void SetActiveScene(Scene* scene) { active_scene_ = scene; }

  WIESEL_GETTER_FN const std::vector<std::unique_ptr<Scene>>& GetLoadedScenes() const {
    return loaded_scenes_;
  }

  MultiScene& GetMultiScene() { return multi_scene_; }

  // Find a loaded scene by name or raw pointer
  WIESEL_GETTER_FN Scene* FindScene(const std::string& name) const;

  // Register a scene file path with a name (e.g. "MainMenu", "Level1")
  void RegisterScene(const std::string& name, const std::string& vfs_path);
  void UnregisterScene(const std::string& name);

  void ClearRegisteredScenes() { registered_scenes_.clear(); }

  const std::map<std::string, std::string>& GetRegisteredScenes() const {
    return registered_scenes_;
  }

  // Synchronous scene loading - loads immediately and returns the scene.
  // Single: replaces all loaded scenes with the new one.
  // Additive: loads on top of existing scenes.
  Scene* LoadScene(const std::string& name,
                   LoadSceneMode mode = LoadSceneMode::Single);
  Scene* LoadSceneFromPath(const std::string& vfs_path,
                           LoadSceneMode mode = LoadSceneMode::Single);

  // Async scene loading - queued for next BeginFrame.
  // Safe to call from scripts during update.
  void LoadSceneAsync(const std::string& name,
                      LoadSceneMode mode = LoadSceneMode::Single);
  void LoadSceneAsyncFromPath(const std::string& vfs_path,
                              LoadSceneMode mode = LoadSceneMode::Single);

  // Unload a specific additively-loaded scene (queued for end of frame)
  void UnloadScene(Scene* scene);
  void UnloadScene(const std::string& name);

  // Immediately unload all additively-loaded scenes (keeps only the active scene)
  void UnloadAllAdditiveScenes();

  // Move an entity (and optionally its children) from one scene to another.
  // Returns the new Entity in the target scene (old entity handle is invalidated).
  Entity MoveEntityToScene(Entity entity, Scene* target_scene,
                           bool move_children = true);

  // Load with an intermediate loading screen scene
  void LoadSceneWithLoading(const std::string& target_scene,
                            const std::string& loading_scene);

  // Query async loading state (for loading screen scripts)
  float GetLoadProgress() const { return load_progress_; }

  bool IsSceneReady() const { return scene_ready_; }

  // Switch to the async-loaded scene (call from loading screen when ready)
  void ActivateLoadedScene();

  // Check if any async scene loads are pending
  bool HasPendingSceneLoad() const { return !pending_async_loads_.empty(); }

  // Rendering
  bool RenderGameView();
  bool RenderEditorView(CameraComponent& camera, TransformComponent& transform,
                        bool show_grid = false);

  // Per-frame lifecycle
  bool BeginFrame();
  void EndFrame();

  void ClearPending() {
    pending_async_loads_.clear();
    pending_unloads_.clear();
    target_scene_path_.clear();
    target_scene_.reset();
    scene_ready_ = false;
    load_progress_ = 0.0f;
    auto_activate_ = false;
  }

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

  Scene* LoadAdditiveFromPath(const std::string& vfs_path,
                              const std::string& name);
  bool ReplacePrimaryScene(const std::string& vfs_path);

  static std::string DeriveNameFromPath(const std::string& vfs_path);
  void UnloadUnusedAssetsForScene(const std::vector<AssetHandle>& scene_assets);

  // Handle ID counter
  uint32_t next_scene_id_ = 1;
  SceneHandle AllocateHandle();

  Scene* active_scene_ = nullptr;
  std::vector<std::unique_ptr<Scene>> loaded_scenes_;
  MultiScene multi_scene_{loaded_scenes_};
  std::map<std::string, std::string> registered_scenes_;

  struct PendingAsyncLoad {
    std::string vfs_path;
    std::string name;
    LoadSceneMode mode;
  };

  std::vector<PendingAsyncLoad> pending_async_loads_;
  // Old scenes kept alive until end of frame, then destroyed
  std::vector<std::unique_ptr<Scene>> pending_unloads_;

  // Async loading state
  std::string target_scene_path_;
  std::unique_ptr<Scene> target_scene_;
  float load_progress_ = 0.0f;
  bool scene_ready_ = false;
  bool auto_activate_ = false;

  // Rendering state
  std::shared_ptr<RenderPipeline> default_pipeline_;
  std::shared_ptr<RenderGraph> external_render_graph_;
  uint32_t pipeline_version_ = 0;
};

}  // namespace wiesel
