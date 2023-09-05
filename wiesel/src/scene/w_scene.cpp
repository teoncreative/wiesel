//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_scene.hpp"

#include "behavior/w_behavior.hpp"
#include "rendering/w_renderer.hpp"
#include "scene/w_entity.hpp"
#include "systems/w_canvas_system.hpp"
#include "w_engine.hpp"

namespace Wiesel {

Scene::Scene() {
  m_CanvasSystem = CreateScope<CanvasSystem>();
}

Scene::~Scene() {}

Entity Scene::CreateEntity(const std::string& name) {
  return CreateEntityWithUUID(UUID(), name);
}

Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
  Entity entity = {m_Registry.create(), this};
  entity.AddComponent<IdComponent>(uuid);
  entity.AddComponent<TransformComponent>();
  entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);

  m_Entities[uuid] = entity;
  m_SceneHierarchy.push_back(entity);
  return entity;
}

void Scene::DestroyEntity(Entity entity) {
  m_Entities.erase(entity.GetUUID());
  m_Registry.destroy(entity);
  if (m_HasCamera && m_CameraEntity == entity) {
    m_HasCamera = false;
  }
  std::remove_if(m_SceneHierarchy.begin(), m_SceneHierarchy.end(), [&](auto& e) {
    return e == entity;
  });
}

void Scene::OnUpdate(float_t deltaTime) {
  for (const auto& entity : m_Registry.view<TransformComponent>()) {
    auto& transform = m_Registry.get<TransformComponent>(entity);
    if (transform.IsChanged) {
      transform.UpdateMatrices();
      transform.IsChanged = false;
      // todo this is a bit hacky
      // set the camera as changed if transform has changed
      if (m_Registry.any_of<CameraComponent>(entity)) {
        auto& camera = m_Registry.get<CameraComponent>(entity);
        camera.m_Camera.m_IsChanged = true;
      }
    }
  }

  m_HasCamera = false;
  for (const auto& entity :
       m_Registry.view<CameraComponent, TransformComponent>()) {
    auto& camera = m_Registry.get<CameraComponent>(entity);
    auto transform = m_Registry.get<TransformComponent>(entity);
    if (m_Registry.any_of<TreeComponent>(entity)) {
      auto& tree = m_Registry.get<TreeComponent>(entity);
      if (tree.Parent != entt::null) {
        ApplyTransform(tree.Parent, transform);
      }
      camera.m_Camera.m_IsChanged = true;
    }
    if (camera.m_Camera.m_IsPrimary) {
      if (camera.m_Camera.m_IsChanged) {
        camera.m_Camera.UpdateProjection();
        camera.m_Camera.UpdateView(transform.Position, transform.Rotation);
        camera.m_Camera.m_IsChanged = false;
      }
      m_CameraEntity = entity;
      if (!m_Camera) {
        m_Camera = CreateReference<CameraData>();
      }
      m_Camera->Position = transform.Position;
      m_Camera->ViewMatrix = camera.m_Camera.m_ViewMatrix;
      m_Camera->Projection = camera.m_Camera.m_Projection;
      m_HasCamera = true;
      break;
    }
  }

  // todo light system
  auto& lights = Engine::GetRenderer()->GetLightsBufferObject();
  lights.DirectLightCount = 0;
  lights.PointLightCount = 0;
  for (const auto& entity : m_Registry.view<LightDirectComponent>()) {
    auto& light = m_Registry.get<LightDirectComponent>(entity);
    auto transform = ApplyTransform(entity);
    UpdateLight(lights, light.LightData, transform);
  }
  for (const auto& entity : m_Registry.view<LightPointComponent>()) {
    auto& light = m_Registry.get<LightPointComponent>(entity);
    auto transform = ApplyTransform(entity);
    UpdateLight(lights, light.LightData, transform);
  }

  for (const auto& entity : m_Registry.view<BehaviorsComponent>()) {
    auto& component = m_Registry.get<BehaviorsComponent>(entity);
    for (const auto& entry : component.m_Behaviors) {
      entry.second->OnUpdate(deltaTime);
    }
  }

  m_CanvasSystem->Update(*this);
}

void Scene::OnEvent(Event& event) {
  EventDispatcher dispatcher{event};
  dispatcher.Dispatch<WindowResizeEvent>(WIESEL_BIND_FN(OnWindowResizeEvent));

  for (const auto& entity : m_Registry.view<BehaviorsComponent>()) {
    auto& component = m_Registry.get<BehaviorsComponent>(entity);
    component.OnEvent(event);
  }
}

void Scene::LinkEntities(entt::entity parent, entt::entity child) {
  entt::entity loopEntity = parent;
  while (loopEntity != entt::null) {
    if (loopEntity == child) {
      return;
    }
    if (!m_Registry.any_of<TreeComponent>(loopEntity)) {
      break;
    }
    auto& tree = m_Registry.get_or_emplace<TreeComponent>(loopEntity);
    loopEntity = tree.Parent;
  }
  auto& parentTree = m_Registry.get_or_emplace<TreeComponent>(parent);
  auto& childTree = m_Registry.get_or_emplace<TreeComponent>(child);
  if (childTree.Parent != entt::null) {
    UnlinkEntities(childTree.Parent, child);
  }
  parentTree.Childs.push_back(child);
  childTree.Parent = parent;
  auto& childTransform = m_Registry.get<TransformComponent>(child);
  auto& parentTransform = m_Registry.get<TransformComponent>(parent);
  glm::vec3 posDiff = childTransform.Position - parentTransform.Position;
  glm::vec3 rotDiff = childTransform.Rotation - parentTransform.Rotation;

  childTransform.Position = posDiff;
  childTransform.Rotation = rotDiff;
  childTransform.IsChanged = true;
}

void Scene::UnlinkEntities(entt::entity parent, entt::entity child) {
  auto& parentTree = m_Registry.get_or_emplace<TreeComponent>(parent);
  auto& childTree = m_Registry.get_or_emplace<TreeComponent>(child);
  if (childTree.Parent == entt::null) {
    return;
  }
  parentTree.Childs.erase(
      std::remove(parentTree.Childs.begin(), parentTree.Childs.end(), child),
      parentTree.Childs.end());
  childTree.Parent = entt::null;
  auto& childTransform = m_Registry.get<TransformComponent>(child);
  auto& parentTransform = m_Registry.get<TransformComponent>(parent);
  glm::vec3 posDiff = childTransform.Position + parentTransform.Position;
  glm::vec3 rotDiff = childTransform.Rotation + parentTransform.Rotation;

  childTransform.Position = posDiff;
  childTransform.Rotation = rotDiff;
  childTransform.IsChanged = true;
}

bool Scene::OnWindowResizeEvent(WindowResizeEvent& event) {
  for (const auto& entity : m_Registry.view<CameraComponent>()) {
    auto& component = m_Registry.get<CameraComponent>(entity);
    component.m_Camera.m_AspectRatio = event.GetAspectRatio();
    component.m_Camera.m_IsChanged = true;
  }
  return false;
}

template <>
void Scene::OnRemoveComponent(entt::entity entity, CameraComponent& component) {
  if (m_CameraEntity == entity) {
    m_HasCamera = false;
  }
}

template <>
void Scene::OnAddComponent(entt::entity entity, CameraComponent& component) {
  if (component.m_Camera.m_IsPrimary) {
    m_CameraEntity = entity;
    m_HasCamera = true;
  }
}

Ref<CameraData> Scene::GetPrimaryCamera() {
  return m_HasCamera ? m_Camera : nullptr;
}

Entity Scene::GetPrimaryCameraEntity() {
  return Entity{m_CameraEntity, this};
}

void Scene::ApplyTransform(entt::entity parent, TransformComponent& childTransform) {
  auto& parentTransform = m_Registry.get<TransformComponent>(parent);
  childTransform.Position += parentTransform.Position;
  childTransform.Rotation += parentTransform.Rotation;
  childTransform.Scale *= parentTransform.Scale;
  childTransform.IsChanged = true;

  if (m_Registry.any_of<TreeComponent>(parent)) {
    auto& tree = m_Registry.get<TreeComponent>(parent);
    if (tree.Parent != entt::null) {
      ApplyTransform(tree.Parent, childTransform);
    }
  }
}

TransformComponent Scene::ApplyTransform(entt::entity entity) {
  auto transform = m_Registry.get<TransformComponent>(entity);
  if (m_Registry.any_of<TreeComponent>(entity)) {
    auto& tree = m_Registry.get<TreeComponent>(entity);
    if (tree.Parent != entt::null) {
      ApplyTransform(tree.Parent, transform);
    }
  }
  return transform;
}

void Scene::Render() {
  // Render models
  for (const auto& entity :
       GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
    auto& model = m_Registry.get<ModelComponent>(entity);
    auto transform = ApplyTransform(entity);
    Engine::GetRenderer()->DrawModel(model, transform);
  }
  m_CanvasSystem->Render(*this);
}

}  // namespace Wiesel