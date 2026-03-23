
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <entt/entt.hpp>

#include "asset/w_asset_handle.hpp"
#include "events/w_appevents.hpp"
#include "ui/w_ui_event_system.hpp"
#include "events/w_engineevents.hpp"
#include "events/w_events.hpp"
#include "rendering/w_camera.hpp"
#include "rendering/w_render_feature.hpp"
#include "rendering/w_rendergraph.hpp"
#include "rendering/w_skybox.hpp"
#include "physics/w_physics_world.hpp"
#include "scene/w_components.hpp"
#include "w_pch.hpp"

namespace Wiesel {

enum class SystemType {
  Update
};

class Entity;
class CanvasSystem;

class Scene {
 public:
  Scene();
  ~Scene();

  Entity CreateEntity(const std::string& name = std::string());
  Entity CreateEntityWithUUID(UUID uuid,
                              const std::string& name = std::string());
  void RemoveEntity(Entity entity);
  entt::entity FindEntityByName(const std::string& name);
  entt::entity FindEntityByUUID(const UUID& uuid);
  std::vector<entt::entity> FindEntitiesByTag(const std::string& tag);

  // Asset dependency tracking — called during deserialization
  void RequestAsset(AssetHandle handle);
  bool AreAssetsReady() const;
  float GetAssetLoadProgress() const;
  const std::vector<AssetHandle>& GetRequestedAssets() const { return requested_assets_; }
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

  void SetSkybox(const std::shared_ptr<Skybox>& skybox) {
    skybox_ = skybox;
  }
  void SetSkyboxAsset(AssetHandle handle);
  AssetHandle GetSkyboxAsset() const {
    return skybox_asset_;
  }
  std::shared_ptr<Skybox> GetSkybox();
  void EnsureDefaultSkybox();
  bool HasCustomSkybox() const {
    return skybox_ != nullptr;
  }

  void SetRenderResolution(glm::vec2 resolution) { render_resolution_ = resolution; }
  glm::vec2 GetRenderResolution() const { return render_resolution_; }

  void SetViewportOrigin(glm::vec2 origin) { viewport_origin_ = origin; }
  glm::vec2 GetViewportOrigin() const { return viewport_origin_; }

  void SetViewportDisplaySize(glm::vec2 size) { viewport_display_size_ = size; }
  glm::vec2 GetViewportDisplaySize() const { return viewport_display_size_; }

  // Set the default render pipeline for all cameras without a per-camera override.
  void SetRenderPipeline(std::shared_ptr<RenderPipeline> pipeline);
  // Set a per-camera render pipeline override.
  void SetRenderPipeline(entt::entity camera, std::shared_ptr<RenderPipeline> pipeline);
  // Create a default pipeline with all built-in features.
  static std::shared_ptr<RenderPipeline> CreateDefaultPipeline(std::shared_ptr<Renderer> renderer);

  std::shared_ptr<RenderPipeline> GetDefaultPipeline() const { return default_pipeline_; }

  template <typename T, typename... Args>
  T& AddComponent(entt::entity handle, Args&&... args) {
    if (HasComponent<T>(handle)) {
      //throw std::runtime_error("Entity already has component!");
      std::terminate();
    }
    auto& component = registry_.emplace<T>(
        handle, std::forward<Args>(args)...);
    OnAddComponent(handle, component);
    return component;
  }

  template <typename T>
  T& GetComponent(entt::entity handle) {  // This function is intentionally not marked as const!
    return registry_.get<T>(handle);
  }

  bool HasEntity(entt::entity handle) const {
    return registry_.valid(handle);
  }

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

  entt::registry& GetRegistry() { return registry_; }
  PhysicsWorld& GetPhysicsWorld() { return *physics_world_; }

  /*
   * Returns the scene hierarchy. This is used by the editor.
   */
  std::vector<entt::entity>& GetSceneHierarchy() { return scene_hierarchy_; }

  void LinkEntities(entt::entity parent, entt::entity child);
  void UnlinkEntities(entt::entity parent, entt::entity child);

  void ProcessDestroyQueue();
  bool Render();
  bool RenderFromExternal(CameraComponent& camera, TransformComponent& transform, bool show_grid = false);
  void BuildRenderGraph(entt::entity camera_entity);
  void InvalidateRenderGraphs();

  // Release all GPU resources (render graphs, camera resource pools, pipelines).
  // Must be called before vkDestroyDevice.
  void Cleanup();

  std::shared_ptr<RenderGraph> GetRenderGraph(entt::entity camera_entity) const {
    auto it = render_graphs_.find(camera_entity);
    return it != render_graphs_.end() ? it->second : nullptr;
  }

  std::shared_ptr<RenderGraph> GetExternalRenderGraph() const {
    return external_render_graph_;
  }

  void ResetPhysicsWorld();
  void ResetScriptStates();
  void ResetFirstUpdate() { first_update_ = true; }

  template <typename Entity, typename... Components, typename Func>
  void BindSystem(SystemType type, Func func) {
    systems_[type].push_back([this, func](float_t delta_time) {
      for (auto handle : registry_.view<Components...>()) {
        Entity entity{handle, this};
        func(delta_time, entity, registry_.get<Components>(handle)...);
      }
    });
  }

  template <typename Entity, typename... Components, typename Func>
  void BindSystem(SystemType type, const std::string& tag, Func func) {
    systems_[type].push_back([this, func, tag](float_t delta_time) {
      for (auto handle : registry_.view<Components...>()) {
        Entity entity{handle, this};
        if (entity.GetName() != tag) {
          continue;
        }
        func(delta_time, entity, registry_.get<Components>(handle)...);
      }
    });
  }


 private:
  bool OnWindowResizeEvent(WindowResizeEvent& event);
  bool OnPipelineRecreatedEvent(PipelineRecreatedEvent& event);
  glm::mat4 MakeLocal(const TransformComponent& transform);
  glm::mat4 GetWorldMatrix(entt::entity entity);
  void UpdateTransforms();
  void UpdateLights();
  void UpdateCameras();
  void UpdateMatrices(entt::entity entity);
  void MarkChildrenDirty(entt::entity entity);
  void UpdateSpriteAnimations(float_t delta_time);
  void UpdateSkeletalAnimations(float_t delta_time);

 private:
  std::unordered_map<UUID, entt::entity> entities_;
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
  bool keep_assets_loaded_ = false;  // If true, assets are never unloaded on scene switch
  bool preload_assets_ = false;      // If true, assets are loaded when the project opens
  UIEventSystem ui_event_system_;
  std::vector<AssetHandle> requested_assets_;
  std::shared_ptr<RenderPipeline> default_pipeline_;
  std::unordered_map<entt::entity, std::shared_ptr<RenderGraph>> render_graphs_;
  std::shared_ptr<RenderGraph> external_render_graph_;
  std::unordered_map<SystemType, std::vector<std::function<void(float_t)>>> systems_;
  std::unique_ptr<PhysicsWorld> physics_world_;
  glm::vec2 render_resolution_{0.0f, 0.0f};
  glm::vec2 viewport_origin_{0.0f, 0.0f};
  glm::vec2 viewport_display_size_{0.0f, 0.0f};

  void UpdateSceneState(float_t delta_time);
};
}  // namespace Wiesel