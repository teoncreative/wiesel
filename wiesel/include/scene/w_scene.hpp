
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

#include "rendering/w_skybox.hpp"
#include "events/w_appevents.hpp"
#include "events/w_events.hpp"
#include "rendering/w_camera.hpp"
#include "scene/w_components.hpp"
#include "w_pch.hpp"

namespace Wiesel {
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

  void OnUpdate(float_t deltaTime);
  void OnEvent(Event& event);

  template <typename T>
  void OnRemoveComponent(entt::entity entity, T& component) {}

  template <typename T>
  void OnAddComponent(entt::entity entity, T& component) {}

  WIESEL_GETTER_FN bool IsRunning() const { return is_running_; }

  WIESEL_GETTER_FN bool IsPaused() const { return is_paused_; }

  void SetPaused(bool paused) { is_paused_ = paused; }

  void SetSkybox(Ref<Skybox> skybox) { skybox_ = skybox; }

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

  /*
   * Returns the scene hierarchy. This is used by the editor.
   */
  std::vector<entt::entity>& GetSceneHierarchy() { return scene_hierarchy_; }

  void LinkEntities(entt::entity parent, entt::entity child);
  void UnlinkEntities(entt::entity parent, entt::entity child);

  void ProcessDestroyQueue();
  bool Render();

 private:
  bool OnWindowResizeEvent(WindowResizeEvent& event);
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
};
}  // namespace Wiesel