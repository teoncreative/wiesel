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
#include "w_engine.hpp"

namespace Wiesel {

Scene::Scene() {
  m_CurrentCamera = CreateReference<CameraData>();
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
  m_SceneHierarchy.erase(std::remove_if(m_SceneHierarchy.begin(), m_SceneHierarchy.end(), [&](auto& e) {
    return e == entity;
  }));
}

void Scene::OnUpdate(float_t deltaTime) {
  if (!m_FirstUpdate) [[likely]] {
    for (const auto& entity : m_Registry.view<BehaviorsComponent>()) {
      auto& component = m_Registry.get<BehaviorsComponent>(entity);
      for (const auto& entry : component.m_Behaviors) {
        entry.second->OnUpdate(deltaTime);
      }
    }
  } else {
    m_FirstUpdate = false;
  }

  for (const auto& entity : m_Registry.view<TransformComponent>()) {
    auto& transform = m_Registry.get<TransformComponent>(entity);
    if (transform.IsChanged) {
      transform.UpdateMatrices();
      transform.IsChanged = false;
      // todo this is a bit hacky
      // set the camera as changed if transform has changed
      if (m_Registry.any_of<CameraComponent>(entity)) {
        auto& camera = m_Registry.get<CameraComponent>(entity);
        camera.IsPosChanged = true;
      }
    }
  }
  auto& lights = Engine::GetRenderer()->m_LightsUniformData;
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

  for (const auto& entity :
       m_Registry.view<CameraComponent, TransformComponent>()) {
    auto& camera = m_Registry.get<CameraComponent>(entity);
    auto transform = m_Registry.get<TransformComponent>(entity);
    if (m_Registry.any_of<TreeComponent>(entity)) {
      auto& tree = m_Registry.get<TreeComponent>(entity);
      if (tree.Parent != entt::null) {
        ApplyTransform(tree.Parent, transform);
      }
      camera.IsPosChanged = true;
    }
    if (!camera.IsEnabled) {
      continue;
    }
    if (camera.IsViewChanged) {
      camera.UpdateProjection();
      camera.IsViewChanged = false;
    }
    if (camera.IsPosChanged) {
      camera.UpdateView(transform.Position, transform.Rotation);
      camera.IsPosChanged = false;
    }
    if (camera.IsAnyChanged) {
      camera.UpdateAll();
      camera.IsAnyChanged = false;
    }
    if (lights.DirectLightCount > 0) {
      camera.ComputeCascades(glm::normalize(lights.DirectLights[0].Direction));
    } else {
      camera.DoesShadowPass = false;
    }
  }
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
    component.AspectRatio = event.GetAspectRatio();
    component.ViewportSize.x = event.GetWindowSize().Width;
    component.ViewportSize.y = event.GetWindowSize().Height;
    component.IsViewChanged = true;
  }
  return false;
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

bool Scene::Render() {
  bool hasCamera = false;
  Ref<Renderer> renderer = Engine::GetRenderer();
  // Render models
  for (const auto& cameraEntity : GetAllEntitiesWith<CameraComponent>()) {
    auto& camera = m_Registry.get<CameraComponent>(cameraEntity);
    auto& cameraTransform = m_Registry.get<TransformComponent>(cameraEntity);
    if (!camera.IsEnabled) {
      continue;
    }
    // Perhaps we could do this differently?
    // At first, we had 3 variables to update, but now we have 18...
    m_CurrentCamera->Position = cameraTransform.Position;
    m_CurrentCamera->ViewMatrix = camera.ViewMatrix;
    m_CurrentCamera->Projection = camera.Projection;
    m_CurrentCamera->InvProjection = camera.InvProjection;
    m_CurrentCamera->ViewportSize = camera.ViewportSize;
    m_CurrentCamera->GeometryColorImage = camera.GeometryColorImage;
    m_CurrentCamera->GeometryDepthStencil = camera.GeometryDepthStencil;
    m_CurrentCamera->GeometryColorResolveImage = camera.GeometryColorResolveImage;
    m_CurrentCamera->CompositeColorImage = camera.CompositeColorImage;
    m_CurrentCamera->CompositeColorResolveImage = camera.CompositeColorResolveImage;
    m_CurrentCamera->LightingColorImage = camera.LightingColorImage;
    m_CurrentCamera->LightingColorResolveImage = camera.LightingColorResolveImage;
    m_CurrentCamera->GeometryFramebuffer = camera.GeometryFramebuffer;
    m_CurrentCamera->LightingFramebuffer = camera.LightingFramebuffer;
    m_CurrentCamera->CompositeFramebuffer = camera.CompositeFramebuffer;
    m_CurrentCamera->Descriptors = camera.Descriptors;
    m_CurrentCamera->ShadowDescriptors = camera.ShadowDescriptors;
    m_CurrentCamera->Planes = camera.Planes;
    m_CurrentCamera->DoesShadowPass = camera.DoesShadowPass;
    m_CurrentCamera->ShadowMapCascades = camera.ShadowMapCascades;
    m_CurrentCamera->ShadowDepthViews = camera.ShadowDepthViews;
    m_CurrentCamera->ShadowDepthStencil = camera.ShadowDepthStencil;
    m_CurrentCamera->ShadowFramebuffers = camera.ShadowFramebuffers;
    m_CurrentCamera->ShadowDepthViewArray = camera.ShadowDepthViewArray;
    m_CurrentCamera->NearPlane = camera.NearPlane;
    m_CurrentCamera->FarPlane = camera.FarPlane;
    renderer->SetCameraData(m_CurrentCamera);
    renderer->BeginFrame();
    if (camera.DoesShadowPass) {
      for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
        renderer->BeginShadowPass(i);
        for (const auto& entity :
             GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
          auto& model = m_Registry.get<ModelComponent>(entity);
          if (!model.Data.ReceiveShadows) {
            continue;
          }
          // We do the transfer twice, one here and one under the geometry pass
          // Maybe if we can find a way to do this more efficiently, that would be better
          auto transform = ApplyTransform(entity);
          renderer->DrawModel(model, transform, true);
        }
        renderer->EndShadowPass();
      }
    } else {
      // we might want to clear the shadow stuff or somehow disable it for later on
    }

    renderer->BeginGeometryPass();
    for (const auto& entity :
         GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
      auto& model = m_Registry.get<ModelComponent>(entity);
      auto transform = ApplyTransform(entity);
      renderer->DrawModel(model, transform, false);
    }
    renderer->EndGeometryPass();
    renderer->BeginLightingPass();
    renderer->GetSkyboxPipeline()->Bind(PipelineBindPointGraphics);
    if (m_Skybox) {
      renderer->DrawSkybox(m_Skybox);
    }
    renderer->GetLightingPipeline()->Bind(PipelineBindPointGraphics);
    renderer->DrawQuad(renderer->GetGeometryColorResolveImage(),
                       renderer->GetLightingPipeline());
    renderer->EndLightingPass();
    renderer->BeginCompositePass();
    renderer->GetLightingPipeline()->Bind(PipelineBindPointGraphics);
    renderer->DrawQuad(renderer->GetLightingColorResolveImage(),
                       renderer->GetCompositePipeline());
    renderer->EndCompositePass();
    renderer->EndFrame();
    hasCamera = true;
  }
  return hasCamera;
}

}  // namespace Wiesel