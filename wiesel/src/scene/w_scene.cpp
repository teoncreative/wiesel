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
  current_camera_ = CreateReference<CameraData>();
}

Scene::~Scene() {}

Entity Scene::CreateEntity(const std::string& name) {
  return CreateEntityWithUUID(UUID(), name);
}

Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
  Entity entity = {registry_.create(), this};
  entity.AddComponent<IdComponent>(uuid);
  entity.AddComponent<TransformComponent>();
  entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);

  entities_[uuid] = entity;
  scene_hierarchy_.push_back(entity);
  return entity;
}

void Scene::RemoveEntity(Entity entity) {
  entities_.erase(entity.GetUUID());
  destroy_queue_.push_back(entity.handle());
  scene_hierarchy_.erase(std::ranges::remove_if(scene_hierarchy_, [&](auto& e) {
                           return e == entity;
                         }).begin());
}

void Scene::DestroyEntity(entt::entity handle) {
  registry_.destroy(handle);
}

void Scene::OnUpdate(float_t deltaTime) {
  PROFILE_ZONE_SCOPED();
  if (!first_update_) [[likely]] {
    for (const auto& entity : registry_.view<BehaviorsComponent>()) {
      auto& component = registry_.get<BehaviorsComponent>(entity);
      for (const auto& entry : component.behaviors_) {
        entry.second->OnUpdate(deltaTime);
      }
    }
  } else {
    first_update_ = false;
  }

  for (const auto& entity : registry_.view<TransformComponent>()) {
    auto& transform = registry_.get<TransformComponent>(entity);
    if (transform.is_changed) {
      UpdateMatrices(entity);
      transform.is_changed = false;
      // todo this is a bit hacky
      // set the camera as changed if transform has changed
      if (registry_.any_of<CameraComponent>(entity)) {
        auto& camera = registry_.get<CameraComponent>(entity);
        camera.pos_changed = true;
      }
    }
  }
  auto& lights = Engine::GetRenderer()->lights_uniform_data_;
  lights.direct_light_count = 0;
  lights.point_light_count = 0;
  for (const auto& entity : registry_.view<LightDirectComponent>()) {
    auto& light = registry_.get<LightDirectComponent>(entity);
    auto& transform = registry_.get<TransformComponent>(entity);
    UpdateLight(lights, light.light_data, transform);
  }
  for (const auto& entity : registry_.view<LightPointComponent>()) {
    auto& light = registry_.get<LightPointComponent>(entity);
    auto& transform = registry_.get<TransformComponent>(entity);
    UpdateLight(lights, light.light_data, transform);
  }

  for (const auto& entity :
       registry_.view<CameraComponent, TransformComponent>()) {
    auto& camera = registry_.get<CameraComponent>(entity);
    auto& transform = registry_.get<TransformComponent>(entity);
    if (!camera.enabled) {
      continue;
    }
    if (camera.view_changed) {
      camera.UpdateProjection();
      camera.view_changed = false;
    }
    if (camera.pos_changed) {
      camera.UpdateView(transform.transform_matrix);
      camera.pos_changed = false;
    }
    if (camera.any_changed) {
      camera.UpdateAll();
      camera.any_changed = false;
    }
    if (lights.direct_light_count > 0) {
      camera.ComputeCascades(glm::normalize(lights.direct_lights[0].direction));
    } else {
      camera.does_shadow_pass = false;
    }
  }
}

void Scene::OnEvent(Event& event) {
  EventDispatcher dispatcher{event};
  dispatcher.Dispatch<WindowResizeEvent>(WIESEL_BIND_FN(OnWindowResizeEvent));

  for (const auto& entity : registry_.view<BehaviorsComponent>()) {
    auto& component = registry_.get<BehaviorsComponent>(entity);
    component.OnEvent(event);
  }
}

void Scene::LinkEntities(entt::entity parent, entt::entity child) {
  entt::entity loop_entity = parent;
  while (loop_entity != entt::null) {
    if (loop_entity == child) {
      return;
    }
    if (!registry_.any_of<TreeComponent>(loop_entity)) {
      break;
    }
    auto& tree = registry_.get_or_emplace<TreeComponent>(loop_entity);
    loop_entity = tree.parent;
  }
  auto& parent_tree = registry_.get_or_emplace<TreeComponent>(parent);
  auto& child_tree = registry_.get_or_emplace<TreeComponent>(child);
  if (child_tree.parent != entt::null) {
    UnlinkEntities(child_tree.parent, child);
  }
  parent_tree.childs.push_back(child);
  child_tree.parent = parent;
  auto& child_transform = registry_.get<TransformComponent>(child);
  auto& parent_transform = registry_.get<TransformComponent>(parent);
  glm::vec3 posDiff = child_transform.position - parent_transform.position;
  glm::vec3 rotDiff = child_transform.rotation - parent_transform.rotation;

  child_transform.position = posDiff;
  child_transform.rotation = rotDiff;
  child_transform.is_changed = true;
}

void Scene::UnlinkEntities(entt::entity parent, entt::entity child) {
  auto& parent_tree = registry_.get_or_emplace<TreeComponent>(parent);
  auto& child_tree = registry_.get_or_emplace<TreeComponent>(child);
  if (child_tree.parent == entt::null) {
    return;
  }
  parent_tree.childs.erase(
      std::ranges::remove(parent_tree.childs, child).begin(),
      parent_tree.childs.end());
  child_tree.parent = entt::null;
  auto& child_transform = registry_.get<TransformComponent>(child);
  auto& parent_transform = registry_.get<TransformComponent>(parent);
  glm::vec3 pos_diff = child_transform.position + parent_transform.position;
  glm::vec3 rot_diff = child_transform.rotation + parent_transform.rotation;

  child_transform.position = pos_diff;
  child_transform.rotation = rot_diff;
  child_transform.is_changed = true;
}

void Scene::ProcessDestroyQueue() {
  PROFILE_ZONE_SCOPED();
  for (const auto& item : destroy_queue_) {
    DestroyEntity(item);
  }
  destroy_queue_.clear();
}

bool Scene::OnWindowResizeEvent(WindowResizeEvent& event) {
  for (const auto& entity : registry_.view<CameraComponent>()) {
    auto& component = registry_.get<CameraComponent>(entity);
    component.aspect_ratio = event.aspect_ratio();
    component.viewport_size.x = event.window_size().width;
    component.viewport_size.y = event.window_size().height;
    component.view_changed = true;
  }
  return false;
}

glm::mat4 Scene::MakeLocal(const TransformComponent& t) {
  PROFILE_ZONE_SCOPED();
  glm::vec3 rotRad = glm::radians(t.rotation);
  glm::mat4 R = glm::toMat4(glm::quat(rotRad));
  glm::mat4 T = glm::translate(glm::mat4(1.0f), t.position);
  glm::mat4 Tp = glm::translate(glm::mat4(1.0f), t.pivot);
  glm::mat4 Tn = glm::translate(glm::mat4(1.0f), -t.pivot);
  glm::mat4 S = glm::scale(glm::mat4(1.0f), t.scale);

  // move to Position, shift to Pivot, rotate+scale, shift back
  return T * Tp * R * S * Tn;
}

glm::mat4 Scene::GetWorldMatrix(entt::entity entity) {
  PROFILE_ZONE_SCOPED();
  auto& transform = registry_.get<TransformComponent>(entity);
  glm::mat4 local = MakeLocal(transform);

  if (auto* tree = registry_.try_get<TreeComponent>(entity);
      tree && tree->parent != entt::null) {
    return GetWorldMatrix(tree->parent) * local;
  }
  return local;
}

void Scene::UpdateMatrices(entt::entity entity) {
  PROFILE_ZONE_SCOPED();
  auto& tc = registry_.get<TransformComponent>(entity);
  tc.transform_matrix = GetWorldMatrix(entity);
  tc.normal_matrix = glm::inverseTranspose(glm::mat3(tc.transform_matrix));
}

bool Scene::Render() {
  PROFILE_ZONE_SCOPED();
  bool hasCamera = false;
  Ref<Renderer> renderer = Engine::GetRenderer();
  // Render models
  for (const auto& cameraEntity : GetAllEntitiesWith<CameraComponent>()) {
    auto& camera = registry_.get<CameraComponent>(cameraEntity);
    auto& camera_transform = registry_.get<TransformComponent>(cameraEntity);
    if (!camera.enabled) {
      continue;
    }
    current_camera_->TransferFrom(camera, camera_transform);
    renderer->SetCameraData(current_camera_);
    renderer->UpdateUniformData();
    if (camera.does_shadow_pass) {
      for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
        PROFILE_GPU_ZONE(renderer->GetTracyCtx(),
                         renderer->GetCommandBuffer().handle_,
                         "Shadow Cascade Pass");
        renderer->BeginShadowPass(i);
        for (const auto& entity :
             GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
          auto& model = registry_.get<ModelComponent>(entity);
          auto& transform = registry_.get<TransformComponent>(entity);
          if (!model.data.receive_shadows || !model.data.enable_rendering) {
            continue;
          }
          renderer->DrawModel(model, transform, true);
        }
        renderer->EndShadowPass();
      }
    }

    {
      PROFILE_GPU_ZONE(renderer->GetTracyCtx(),
                       renderer->GetCommandBuffer().handle_, "Geometry Pass");
      renderer->BeginGeometryPass();
      for (const auto& entity :
           GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
        auto& model = registry_.get<ModelComponent>(entity);
        auto& transform = registry_.get<TransformComponent>(entity);
        if (!model.data.enable_rendering) {
          continue;
        }
        renderer->DrawModel(model, transform, false);
      }
      renderer->EndGeometryPass();
    }
    if (renderer->IsSSAOEnabled()) {
      {
        PROFILE_GPU_ZONE(renderer->GetTracyCtx(),
                         renderer->GetCommandBuffer().handle_, "SSAO Gen Pass");
        renderer->BeginSSAOGenPass();
        renderer->GetSSAOGenPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetSSAOGenPipeline(),
            {renderer->GetCameraData()->ssao_gen_descriptor,
             renderer->GetCameraData()->global_descriptor});
        renderer->EndSSAOGenPass();
      }
      {
        PROFILE_GPU_ZONE(renderer->GetTracyCtx(),
                         renderer->GetCommandBuffer().handle_,
                         "SSAO Blur Pass");
        renderer->BeginSSAOBlurHorzPass();
        renderer->GetSSAOBlurHorzPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetSSAOBlurHorzPipeline(),
            {renderer->GetCameraData()->ssao_output_descriptor});
        renderer->EndSSAOBlurHorzPass();
        renderer->BeginSSAOBlurVertPass();
        renderer->GetSSAOBlurVertPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetSSAOBlurVertPipeline(),
            {renderer->GetCameraData()->ssao_blur_horz_output_descriptor});
        renderer->EndSSAOBlurVertPass();
      }
    }
    {
      PROFILE_GPU_ZONE(renderer->GetTracyCtx(),
                       renderer->GetCommandBuffer().handle_, "Lighting Pass");
      renderer->BeginLightingPass();
      renderer->GetSkyboxPipeline()->Bind(PipelineBindPointGraphics);
      if (skybox_) {
        renderer->DrawSkybox(skybox_);
      }
      renderer->GetLightingPipeline()->Bind(PipelineBindPointGraphics);
      renderer->DrawFullscreen(
          renderer->GetLightingPipeline(),
          {renderer->GetCameraData()->geometry_output_descriptor,
           renderer->GetCameraData()->ssao_blur_vert_output_descriptor,
           renderer->GetCameraData()->global_descriptor});
      renderer->EndLightingPass();
    }
    {
      PROFILE_GPU_ZONE(renderer->GetTracyCtx(),
                       renderer->GetCommandBuffer().handle_, "Sprite Pass");
      renderer->BeginSpritePass();
      renderer->GetSpritePipeline()->Bind(PipelineBindPointGraphics);
      for (const auto& entity :
           GetAllEntitiesWith<SpriteComponent, TransformComponent>()) {
        auto& sprite = registry_.get<SpriteComponent>(entity);
        auto& transform = registry_.get<TransformComponent>(entity);
        renderer->DrawSprite(sprite, transform);
      }
      renderer->EndSpritePass();
    }
    {
      PROFILE_GPU_ZONE(renderer->GetTracyCtx(),
                       renderer->GetCommandBuffer().handle_, "Composite Pass");
      renderer->BeginCompositePass();
      renderer->GetCompositePipeline()->Bind(PipelineBindPointGraphics);
      if (renderer->IsOnlySSAO()) {
        renderer->DrawFullscreen(
            renderer->GetCompositePipeline(),
            {renderer->GetCameraData()->ssao_blur_vert_output_descriptor});
      } else {
        renderer->DrawFullscreen(
            renderer->GetCompositePipeline(),
            {renderer->GetCameraData()->lighting_output_descriptor});
        renderer->DrawFullscreen(
            renderer->GetCompositePipeline(),
            {renderer->GetCameraData()->sprite_output_descriptor});
      }
      renderer->EndCompositePass();
    }
    hasCamera = true;
  }
  return hasCamera;
}

}  // namespace Wiesel