
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

#include "events/w_appevents.hpp"
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

  void SetSkybox(Ref<Skybox> skybox) { skybox_ = skybox; }
  Ref<Skybox> GetSkybox() const { return skybox_; }

  void SetRenderResolution(glm::vec2 resolution) { render_resolution_ = resolution; }
  glm::vec2 GetRenderResolution() const { return render_resolution_; }

  // Set the default render pipeline for all cameras without a per-camera override.
  void SetRenderPipeline(Ref<RenderPipeline> pipeline);
  // Set a per-camera render pipeline override.
  void SetRenderPipeline(entt::entity camera, Ref<RenderPipeline> pipeline);
  // Create a default pipeline with all built-in features.
  static Ref<RenderPipeline> CreateDefaultPipeline(Ref<Renderer> renderer);

  Ref<RenderPipeline> GetDefaultPipeline() const { return default_pipeline_; }

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
  bool RenderFromExternal(CameraComponent& camera, TransformComponent& transform);
  void BuildRenderGraph(entt::entity camera_entity);
  void InvalidateRenderGraphs();

  // Release all GPU resources (render graphs, camera resource pools, pipelines).
  // Must be called before vkDestroyDevice.
  void Cleanup();

  Ref<RenderGraph> GetRenderGraph(entt::entity camera_entity) const {
    auto it = render_graphs_.find(camera_entity);
    return it != render_graphs_.end() ? it->second : nullptr;
  }

  Ref<RenderGraph> GetExternalRenderGraph() const {
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
  void UpdateMatrices(entt::entity entity);
  void DestroyEntity(entt::entity handle);

 private:
  std::unordered_map<UUID, entt::entity> entities_;
  entt::registry registry_;
  bool is_running_ = false;
  bool is_paused_ = false;
  bool first_update_ = true;
  std::vector<entt::entity> scene_hierarchy_;
  std::vector<entt::entity> destroy_queue_;
  // this camera is used to render the scene to the current camera
  Ref<CameraData> current_camera_;
  Ref<Skybox> skybox_;
  Ref<RenderPipeline> default_pipeline_;
  std::unordered_map<entt::entity, Ref<RenderGraph>> render_graphs_;
  Ref<RenderGraph> external_render_graph_;
  std::unordered_map<SystemType, std::vector<std::function<void(float_t)>>> systems_;
  std::unique_ptr<PhysicsWorld> physics_world_;
  glm::vec2 render_resolution_{0.0f, 0.0f};

  void UpdateSceneState(float_t delta_time);
};
}  // namespace Wiesel