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
  {
    auto entity = CreateEntity("Directional Light");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Position = glm::vec3(1.0f, 1.0f, 1.0f);
    entity.AddComponent<LightDirectComponent>();
  }
  {
    auto entity = CreateEntity("Point Light");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Position = glm::vec3{0.0f, 1.0f, 0.0f};
    entity.AddComponent<LightPointComponent>();
  }
  {
    auto entity = CreateEntity("Camera");
    auto& camera = entity.AddComponent<CameraComponent>();
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Position = glm::vec3(0.0f, 1.0f, 0.0f);
    camera.m_Camera.m_AspectRatio = Engine::GetRenderer()->GetAspectRatio();
    camera.m_Camera.m_IsPrimary = true;
    camera.m_Camera.m_IsChanged = true;
  }
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
  auto& tag = entity.AddComponent<TagComponent>();
  tag.Tag = name.empty() ? "Entity" : name;

  m_Entities[uuid] = entity;
  return entity;
}

void Scene::DestroyEntity(Entity entity) {
  m_Entities.erase(entity.GetUUID());
  m_Registry.destroy(entity);
  if (m_HasCamera && m_CameraEntity == entity) {
    m_HasCamera = false;
  }
}

void Scene::OnUpdate(float_t deltaTime) {
  for (const auto& entity : m_Registry.view<BehaviorsComponent>()) {
    auto& component = m_Registry.get<BehaviorsComponent>(entity);
    for (const auto& entry : component.m_Behaviors) {
      if (!entry.second->IsEnabled()) {
        continue;
      }
      entry.second->OnUpdate(deltaTime);
    }
  }

  for (const auto& entity : m_Registry.view<TransformComponent>()) {
    auto& transform = m_Registry.get<TransformComponent>(entity);
    if (transform.IsChanged) {
      transform.UpdateMatrices();
      transform.IsChanged = false;
    }
  }

  bool cameraFound = false;
  for (const auto& entity :
       m_Registry.view<CameraComponent, TransformComponent>()) {
    auto& camera = m_Registry.get<CameraComponent>(entity);
    auto& transform = m_Registry.get<TransformComponent>(entity);
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
      cameraFound = true;
      break;
    }
  }
  m_HasCamera = cameraFound;

  // todo light system
  auto& lights = Engine::GetRenderer()->GetLightsBufferObject();
  lights.DirectLightCount = 0;
  lights.PointLightCount = 0;
  for (const auto& entity : m_Registry.view<LightDirectComponent>()) {
    auto& light = m_Registry.get<LightDirectComponent>(entity);
    UpdateLight(lights, light.LightData, {entity, this});
  }
  for (const auto& entity : m_Registry.view<LightPointComponent>()) {
    auto& light = m_Registry.get<LightPointComponent>(entity);
    UpdateLight(lights, light.LightData, {entity, this});
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

Ref<CameraData> Scene::GetPrimaryCamera() {
  return m_HasCamera ? m_Camera : nullptr;
}

Entity Scene::GetPrimaryCameraEntity() {
  return Entity{m_CameraEntity, this};
}

void Scene::Render() {
  // Render models
  for (const auto& entity :
       GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
    auto& model = m_Registry.get<ModelComponent>(entity);
    auto& transform = m_Registry.get<TransformComponent>(entity);
    Engine::GetRenderer()->DrawModel(model, transform);
  }
  m_CanvasSystem->Render(*this);
}

}  // namespace Wiesel