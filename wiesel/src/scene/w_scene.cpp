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
#include <rendering/w_sprite.hpp>

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
      UpdateMatrices(entity);
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
    auto& transform = m_Registry.get<TransformComponent>(entity);
    UpdateLight(lights, light.LightData, transform);
  }
  for (const auto& entity : m_Registry.view<LightPointComponent>()) {
    auto& light = m_Registry.get<LightPointComponent>(entity);
    auto& transform = m_Registry.get<TransformComponent>(entity);
    UpdateLight(lights, light.LightData, transform);
  }

  for (const auto& entity :
       m_Registry.view<CameraComponent, TransformComponent>()) {
    auto& camera = m_Registry.get<CameraComponent>(entity);
    auto& transform = m_Registry.get<TransformComponent>(entity);
    if (!camera.IsEnabled) {
      continue;
    }
    if (camera.IsViewChanged) {
      camera.UpdateProjection();
      camera.IsViewChanged = false;
    }
    if (camera.IsPosChanged) {
      camera.UpdateView(transform.TransformMatrix);
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

glm::mat4 Scene::MakeLocal(const Wiesel::TransformComponent& t) {
  glm::vec3 rotRad = glm::radians(t.Rotation);
  glm::mat4 R      = glm::toMat4(glm::quat(rotRad));
  glm::mat4 T      = glm::translate(glm::mat4(1.0f), t.Position);
  glm::mat4 Tp     = glm::translate(glm::mat4(1.0f), t.Pivot);
  glm::mat4 Tn     = glm::translate(glm::mat4(1.0f), -t.Pivot);
  glm::mat4 S      = glm::scale(glm::mat4(1.0f), t.Scale);

  // move to Position, shift to Pivot, rotate+scale, shift back
  return T * Tp * R * S * Tn;
}

glm::mat4 Scene::GetWorldMatrix(entt::entity entity) {
  auto& transform = m_Registry.get<TransformComponent>(entity);
  glm::mat4 local = MakeLocal(transform);

  if (auto* tree = m_Registry.try_get<TreeComponent>(entity);
      tree && tree->Parent != entt::null) {
    return GetWorldMatrix(tree->Parent) * local;
  }
  return local;
}

void Scene::UpdateMatrices(entt::entity entity) {
  auto& tc = m_Registry.get<TransformComponent>(entity);
  tc.TransformMatrix = GetWorldMatrix(entity);
  tc.NormalMatrix    = glm::inverseTranspose(glm::mat3(tc.TransformMatrix));
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
    m_CurrentCamera->TransferFrom(camera, cameraTransform);
    renderer->SetCameraData(m_CurrentCamera);
    renderer->BeginFrame();
    if (camera.DoesShadowPass) {
      for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
        renderer->BeginShadowPass(i);
        for (const auto& entity :
             GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
          auto& model = m_Registry.get<ModelComponent>(entity);
          auto& transform = m_Registry.get<TransformComponent>(entity);
          if (!model.Data.ReceiveShadows) {
            continue;
          }
          renderer->DrawModel(model, transform, true);
        }
        renderer->EndShadowPass();
      }
    }

    renderer->BeginGeometryPass();
    for (const auto& entity :
         GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
      auto& model = m_Registry.get<ModelComponent>(entity);
      auto& transform = m_Registry.get<TransformComponent>(entity);
      renderer->DrawModel(model, transform, false);
    }
    renderer->EndGeometryPass();
    if (renderer->IsSSAOEnabled()) {
      renderer->BeginSSAOGenPass();
      renderer->GetSSAOGenPipeline()->Bind(PipelineBindPointGraphics);
      renderer->DrawFullscreen(renderer->GetSSAOGenPipeline(), {renderer->GetCameraData()->SSAOGenDescriptor,
                                                                renderer->GetCameraData()->GlobalDescriptor});
      renderer->EndSSAOGenPass();
      renderer->BeginSSAOBlurPass();
      renderer->GetSSAOBlurPipeline()->Bind(PipelineBindPointGraphics);
      renderer->DrawFullscreen(renderer->GetSSAOBlurPipeline(), {renderer->GetCameraData()->SSAOOutputDescriptor});
      renderer->EndSSAOBlurPass();
    }
    renderer->BeginLightingPass();
    renderer->GetSkyboxPipeline()->Bind(PipelineBindPointGraphics);
    if (m_Skybox) {
      renderer->DrawSkybox(m_Skybox);
    }
    renderer->GetLightingPipeline()->Bind(PipelineBindPointGraphics);
    renderer->DrawFullscreen(renderer->GetLightingPipeline(), {renderer->GetCameraData()->GeometryOutputDescriptor,
                                                               renderer->GetCameraData()->SSAOBlurOutputDescriptor,
                                                              renderer->GetCameraData()->GlobalDescriptor});
    renderer->EndLightingPass();
    renderer->BeginSpritePass();
    renderer->GetSpritePipeline()->Bind(PipelineBindPointGraphics);
    for (const auto& entity :
         GetAllEntitiesWith<SpriteComponent, TransformComponent>()) {
      auto& sprite = m_Registry.get<SpriteComponent>(entity);
      auto& transform = m_Registry.get<TransformComponent>(entity);
      renderer->DrawSprite(sprite, transform);
    }
    renderer->EndSpritePass();
    renderer->BeginCompositePass();
    renderer->GetCompositePipeline()->Bind(PipelineBindPointGraphics);
    renderer->DrawFullscreen(renderer->GetCompositePipeline(), {renderer->GetCameraData()->LightingOutputDescriptor});
    renderer->DrawFullscreen(renderer->GetCompositePipeline(), {renderer->GetCameraData()->SpriteOutputDescriptor});
    renderer->EndCompositePass();
    renderer->EndFrame();
    hasCamera = true;
  }
  return hasCamera;
}

}  // namespace Wiesel