
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <entt/entt.hpp>

#include "asset/w_asset_handle.h"
#include "scene/w_scene_handle.h"
#include "events/w_appevents.h"
#include "events/w_engineevents.h"
#include "events/w_events.h"
#include "physics/w_physics_world.h"
#include "rendering/w_camera.h"
#include "rendering/w_skybox.h"
#include "systems/w_system.h"
#include "ui/w_ui_event_system.h"
#include "w_pch.h"

namespace wiesel {

class Entity;
class CanvasSystem;
class MultiScene;

class Scene {
 public:
  Scene();
  ~Scene();

  SceneHandle GetHandle() const { return handle_; }

  const std::string& GetName() const { return name_; }

  void SetName(const std::string& name) { name_ = name; }

  const std::string& GetSourcePath() const { return source_path_; }

  void SetSourcePath(const std::string& path) { source_path_ = path; }

  Entity CreateEntity(const std::string& name = std::string());
  Entity CreateEntityWithUUID(urkern::UUID uuid,
                              const std::string& name = std::string());
  void RemoveEntity(entt::entity entity);
  entt::entity FindEntityByName(const std::string& name);
  entt::entity FindEntityByUUID(const urkern::UUID& uuid);
  std::vector<entt::entity> FindEntitiesByTag(const std::string& tag);

  // Asset dependency tracking — called during deserialization
  void RequestAsset(AssetHandle handle);
  bool AreAssetsReady() const;
  float GetAssetLoadProgress() const;

  const std::vector<AssetHandle>& GetRequestedAssets() const {
    return requested_assets_;
  }

  void ClearRequestedAssets();

  bool GetKeepAssetsLoaded() const { return keep_assets_loaded_; }

  void SetKeepAssetsLoaded(bool keep) { keep_assets_loaded_ = keep; }

  bool GetPreloadAssets() const { return preload_assets_; }

  void SetPreloadAssets(bool preload) { preload_assets_ = preload; }

  void OnUpdate(float_t delta_time);
  void OnUpdateEditor(float_t delta_time);
  void OnEvent(Event& event);

  template <typename T>
  void OnRemoveComponent(entt::entity entity, T& component) {}

  template <typename T>
  void OnAddComponent(entt::entity entity, T& component) {}

  WIESEL_GETTER_FN bool IsRunning() const { return is_running_; }

  WIESEL_GETTER_FN bool IsPaused() const { return is_paused_; }

  void SetPaused(bool paused) { is_paused_ = paused; }

  void SetSkybox(const std::shared_ptr<Skybox>& skybox) { skybox_ = skybox; }

  void SetSkyboxAsset(AssetHandle handle);

  AssetHandle GetSkyboxAsset() const { return skybox_asset_; }

  std::shared_ptr<Skybox> GetSkybox();
  void EnsureDefaultSkybox();

  bool HasCustomSkybox() const { return skybox_ != nullptr; }

  void SetCursorSetAsset(AssetHandle handle);

  AssetHandle GetCursorSetAsset() const { return cursor_set_asset_; }

  void SetRenderResolution(glm::vec2 resolution) {
    render_resolution_ = resolution;
  }

  glm::vec2 GetRenderResolution() const { return render_resolution_; }

  void SetViewportOrigin(glm::vec2 origin) { viewport_origin_ = origin; }

  glm::vec2 GetViewportOrigin() const { return viewport_origin_; }

  void SetViewportDisplaySize(glm::vec2 size) { viewport_display_size_ = size; }

  glm::vec2 GetViewportDisplaySize() const { return viewport_display_size_; }

  // Set a per-camera render pipeline override.
  void SetRenderPipeline(entt::entity camera,
                         std::shared_ptr<RenderPipeline> pipeline);

  template <typename T, typename... Args>
  T& AddComponent(entt::entity handle, Args&&... args) {
    if (HasComponent<T>(handle)) {
      throw std::runtime_error("Entity already has component!");
    }
    auto& component = registry_.emplace<T>(handle, std::forward<Args>(args)...);
    OnAddComponent(handle, component);
    return component;
  }

  template <typename T>
  T& GetComponent(entt::entity handle) {
    // This function is intentionally not marked as const!
    return registry_.get<T>(handle);
  }

  bool HasEntity(entt::entity handle) const { return registry_.valid(handle); }

  template <typename T>
  bool HasComponent(entt::entity handle) const {
    return registry_.any_of<T>(handle);
  }

  template <typename T>
  void RemoveComponent(entt::entity handle) {
    if (!HasComponent<T>(handle)) {
      return;
    }
    auto& component = GetComponent<T>(handle);
    OnRemoveComponent<T>(handle, component);
    registry_.remove<T>(handle);
  }

  template <typename... Components>
  auto GetAllEntitiesWith() {
    return registry_.view<Components...>();
  }

  bool IsValid(entt::entity entity) const { return registry_.valid(entity); }

  entt::registry& GetRegistry() { return registry_; }

  PhysicsWorld& GetPhysicsWorld() { return *physics_world_; }

  UIEventSystem& GetUIEventSystem() { return ui_event_system_; }

  /*
   * Returns the scene hierarchy. This is used by the editor.
   */
  std::vector<entt::entity>& GetSceneHierarchy() { return scene_hierarchy_; }

  void LinkEntities(entt::entity parent, entt::entity child,
                    bool convert_to_local = true);
  void UnlinkEntities(entt::entity parent, entt::entity child);

  // Instantiate a Model asset as an entity hierarchy.
  // Creates entities for each node, adds MeshRenderer/SkinnedMeshRenderer
  // on mesh nodes. Returns the root entity.
  Entity InstantiateModel(AssetHandle model_handle,
                          const std::string& name = "");

  void ProcessDestroyQueue();

  std::shared_ptr<CameraData> GetCurrentCamera() { return current_camera_; }

  void ResetPhysicsWorld();
  void ResetScriptStates();

  void ResetFirstUpdate() { first_update_ = true; }

  // Register an ECS system. Systems are executed in priority order each frame.
  void AddSystem(std::unique_ptr<ISystem> system);

  template <typename T, typename... Args>
  T& AddSystem(Args&&... args) {
    auto system = std::make_unique<T>(std::forward<Args>(args)...);
    T& ref = *system;
    AddSystem(std::move(system));
    return ref;
  }

  template <typename T>
  T* GetSystem() {
    for (auto& system : systems_) {
      if (auto* cast = dynamic_cast<T*>(system.get())) {
        return cast;
      }
    }
    return nullptr;
  }

 private:
  bool OnWindowResizeEvent(WindowResizedEvent& event);

 private:
  friend class SceneManager;
  SceneHandle handle_;
  std::string name_;
  std::string source_path_;
  std::unordered_map<urkern::UUID, entt::entity> entities_;
  entt::registry registry_;
  bool is_running_{false};
  bool is_paused_{false};
  bool first_update_{true};
  std::vector<entt::entity> scene_hierarchy_;
  std::vector<entt::entity> destroy_queue_;
  // This camera is used to render the scene to the current camera
  std::shared_ptr<CameraData> current_camera_;
  std::shared_ptr<Skybox> skybox_;
  std::shared_ptr<Skybox> default_skybox_;
  AssetHandle skybox_asset_;
  AssetHandle cursor_set_asset_;
  bool keep_assets_loaded_ =
      false;  // If true, assets are never unloaded on scene switch
  bool preload_assets_ =
      false;  // If true, assets are loaded when the project opens
  UIEventSystem ui_event_system_;
  std::vector<AssetHandle> requested_assets_;
  std::vector<std::unique_ptr<ISystem>> systems_;
  std::unique_ptr<PhysicsWorld> physics_world_;
  glm::vec2 render_resolution_{0.0f, 0.0f};
  glm::vec2 viewport_origin_{0.0f, 0.0f};
  glm::vec2 viewport_display_size_{0.0f, 0.0f};
};

// Lightweight wrapper over multiple Scene pointers.
// Provides cross-scene entity iteration via ForEach.
class MultiScene {
 public:
  MultiScene() = delete;

  explicit MultiScene(std::vector<std::unique_ptr<Scene>>& scenes)
      : scenes_(scenes) {}

  MultiScene(MultiScene&) = delete;

  // Iterate entities with given components across all scenes.
  // Callback signature: void(Scene& scene, entt::entity entity)
  // Automatically sets renderer scene index per-scene for entity picking.
  template <typename... Components, typename Func>
  void ForEach(Func func) {
    for (size_t i = 0; i < scenes_.size(); ++i) {
      SetSceneIndex(static_cast<uint8_t>(i));
      Scene& scene = *scenes_[i];
      for (entt::entity entity : scene.GetAllEntitiesWith<Components...>()) {
        func(scene, entity);
      }
    }
  }

  // Primary scene (first in list - used for skybox, pipeline, etc.)
  Scene& primary() { return *scenes_.front(); }

  bool empty() const { return scenes_.empty(); }

  size_t size() const { return scenes_.size(); }

 private:
  // Defined in .cc to avoid including w_engine.h
  static void SetSceneIndex(uint8_t index);

  std::vector<std::unique_ptr<Scene>>& scenes_;
};

}  // namespace wiesel