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

#include <ranges>
#include <rendering/w_sprite.hpp>

#include "behavior/w_behavior.hpp"
#include "rendering/w_render_feature.hpp"
#include "rendering/w_renderer.hpp"
#include "rendering/features/w_shadow_feature.hpp"
#include "rendering/features/w_geometry_feature.hpp"
#include "rendering/features/w_ssao_feature.hpp"
#include "rendering/features/w_lighting_feature.hpp"
#include "rendering/features/w_sprite_feature.hpp"
#include "rendering/features/w_composite_feature.hpp"
#include "rendering/features/w_taa_feature.hpp"
#include "rendering/features/w_bloom_feature.hpp"
#include "rendering/features/w_motion_blur_feature.hpp"
#include "rendering/features/w_fxaa_feature.hpp"
#include "scene/w_entity.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "w_engine.hpp"

namespace Wiesel {
class PipelineRecreatedEvent;

Scene::Scene() {
  current_camera_ = CreateReference<CameraData>();
  physics_world_ = std::make_unique<PhysicsWorld>(this);
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

void Scene::OnUpdate(float_t delta_time) {
  PROFILE_ZONE_SCOPED();
  if (!first_update_) [[likely]] {
    // Create bodies for new entities before scripts run
    physics_world_->EnsureBodiesExist();

    for (const auto& entity : registry_.view<BehaviorsComponent>()) {
      BehaviorsComponent& component = registry_.get<BehaviorsComponent>(entity);
      for (IBehavior*& value : component.behaviors_ | std::views::values) {
        value->OnUpdate(delta_time);
      }
    }
    for (auto&& fn : systems_[SystemType::Update]) {
      fn(delta_time);
    }

    // Physics step
    physics_world_->SyncTransformsFromECS();
    physics_world_->StepSimulation(delta_time);
    physics_world_->SyncTransformsToECS();
    physics_world_->DetectContacts();
  } else {
    first_update_ = false;
  }

  UpdateSceneState(delta_time);
}

void Scene::OnUpdateEditor(float_t delta_time) {
  PROFILE_ZONE_SCOPED();
  UpdateSceneState(delta_time);
}

void Scene::UpdateSceneState(float_t delta_time) {
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
  dispatcher.Dispatch<PipelineRecreatedEvent>(
      WIESEL_BIND_FN(OnPipelineRecreatedEvent));

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
    physics_world_->DestroyBody(item);
    DestroyEntity(item);
  }
  destroy_queue_.clear();
}

bool Scene::OnWindowResizeEvent(WindowResizeEvent& event) {
  /*for (const auto& entity : registry_.view<CameraComponent>()) {
    auto& component = registry_.get<CameraComponent>(entity);
    component.aspect_ratio = event.aspect_ratio();
    component.viewport_size.x = event.window_size().width;
    component.viewport_size.y = event.window_size().height;
    component.resources_dirty = true;
    component.view_changed = true;
  }
  return false;*/
  return false;
}

bool Scene::OnPipelineRecreatedEvent(PipelineRecreatedEvent& event) {
  for (const auto& entity : registry_.view<CameraComponent>()) {
    auto& component = registry_.get<CameraComponent>(entity);
    component.resources_dirty = true;
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

void Scene::InvalidateRenderGraphs() {
  render_graphs_.clear();
}

void Scene::BuildRenderGraph(entt::entity camera_entity) {
  Ref<Renderer> renderer = Engine::GetRenderer();
  auto& camera = registry_.get<CameraComponent>(camera_entity);

  std::shared_ptr<RenderGraph>& graph = render_graphs_[camera_entity];
  graph = CreateReference<RenderGraph>(*renderer);

  bool use_resolve = renderer->options().msaa_mode > SamplingMode::DISABLED;
  RenderContext ctx{*renderer, *this, camera, camera.resource_pool,
                    camera.viewport_size, use_resolve};

  auto& pipeline = camera.render_pipeline ? *camera.render_pipeline
                                          : *default_pipeline_;
  pipeline.BuildRenderGraph(*graph, ctx);
  graph->Compile();
}

bool Scene::Render() {
  PROFILE_ZONE_SCOPED();
  bool hasCamera = false;
  Ref<Renderer> renderer = Engine::GetRenderer();

  // Ensure we have a default pipeline
  if (!default_pipeline_) {
    default_pipeline_ = CreateDefaultPipeline(renderer);
  }

  for (const auto& cameraEntity : GetAllEntitiesWith<CameraComponent>()) {
    auto& camera = registry_.get<CameraComponent>(cameraEntity);
    auto& camera_transform = registry_.get<TransformComponent>(cameraEntity);
    if (!camera.enabled)
      continue;

    if (camera.resources_dirty) {
      vkDeviceWaitIdle(renderer->GetLogicalDevice());
      bool use_resolve = renderer->options().msaa_mode > SamplingMode::DISABLED;
      RenderContext ctx{*renderer, *this, camera, camera.resource_pool,
                        camera.viewport_size, use_resolve};
      auto& pipeline = camera.render_pipeline ? *camera.render_pipeline
                                              : *default_pipeline_;
      pipeline.SetupResources(ctx);
      camera.resources_dirty = false;
      render_graphs_.erase(cameraEntity);
    }

    current_camera_->TransferFrom(camera, camera_transform);
    renderer->SetCameraData(current_camera_);
    renderer->UpdateUniformData();

    // Rebuild render graph each frame to pick up settings changes
    BuildRenderGraph(cameraEntity);
    render_graphs_[cameraEntity]->Execute(renderer->GetCommandBuffer().handle_);

    // Store current VP for next frame's motion blur
    camera.prev_view_projection = camera.projection * camera.view_matrix;

    hasCamera = true;
  }
  return hasCamera;
}

bool Scene::RenderFromExternal(CameraComponent& camera,
                               TransformComponent& transform) {
  PROFILE_ZONE_SCOPED();
  Ref<Renderer> renderer = Engine::GetRenderer();

  if (!default_pipeline_) {
    default_pipeline_ = CreateDefaultPipeline(renderer);
  }

  // Compute transform matrix (no entity hierarchy for external camera)
  glm::vec3 rotRad = glm::radians(transform.rotation);
  glm::mat4 R = glm::toMat4(glm::quat(rotRad));
  glm::mat4 T = glm::translate(glm::mat4(1.0f), transform.position);
  glm::mat4 S = glm::scale(glm::mat4(1.0f), transform.scale);
  transform.transform_matrix = T * R * S;

  // Update camera matrices
  if (camera.view_changed) {
    camera.UpdateProjection();
    camera.view_changed = false;
  }
  camera.UpdateView(transform.transform_matrix);
  camera.UpdateAll();

  // Setup resources if dirty
  if (camera.resources_dirty) {
    vkDeviceWaitIdle(renderer->GetLogicalDevice());
    bool use_resolve = renderer->options().msaa_mode > SamplingMode::DISABLED;
    RenderContext ctx{*renderer, *this, camera, camera.resource_pool,
                      camera.viewport_size, use_resolve};
    auto& pipeline = camera.render_pipeline ? *camera.render_pipeline
                                            : *default_pipeline_;
    pipeline.SetupResources(ctx);
    camera.resources_dirty = false;
    external_render_graph_ = nullptr;
  }

  current_camera_->TransferFrom(camera, transform);
  renderer->SetCameraData(current_camera_);
  renderer->UpdateUniformData();

  // Build and execute render graph
  external_render_graph_ = CreateReference<RenderGraph>(*renderer);
  bool use_resolve = renderer->options().msaa_mode > SamplingMode::DISABLED;
  RenderContext ctx{*renderer, *this, camera, camera.resource_pool,
                    camera.viewport_size, use_resolve};
  auto& pipeline = camera.render_pipeline ? *camera.render_pipeline
                                          : *default_pipeline_;
  pipeline.BuildRenderGraph(*external_render_graph_, ctx);
  external_render_graph_->Compile();
  external_render_graph_->Execute(renderer->GetCommandBuffer().handle_);

  camera.prev_view_projection = camera.projection * camera.view_matrix;
  return true;
}

void Scene::ResetPhysicsWorld() {
  glm::vec3 gravity = physics_world_->GetGravity();
  physics_world_.reset();
  physics_world_ = std::make_unique<PhysicsWorld>(this);
  physics_world_->SetGravity(gravity);
}

void Scene::ResetScriptStates() {
  for (const auto& entity : registry_.view<BehaviorsComponent>()) {
    auto& component = registry_.get<BehaviorsComponent>(entity);
    for (auto& [name, behavior] : component.behaviors_) {
      if (auto* mono = dynamic_cast<MonoBehavior*>(behavior)) {
        if (auto* instance = mono->script_instance()) {
          instance->ResetStartState();
        }
      }
    }
  }
}

void Scene::SetRenderPipeline(Ref<RenderPipeline> pipeline) {
  default_pipeline_ = std::move(pipeline);
  // Invalidate all camera resources so they get rebuilt with the new pipeline
  for (const auto& entity : registry_.view<CameraComponent>()) {
    auto& camera = registry_.get<CameraComponent>(entity);
    if (!camera.render_pipeline) {
      camera.resource_pool.Clear();
      camera.resources_dirty = true;
    }
  }
}

void Scene::SetRenderPipeline(entt::entity camera_entity,
                              Ref<RenderPipeline> pipeline) {
  auto& camera = registry_.get<CameraComponent>(camera_entity);
  camera.render_pipeline = std::move(pipeline);
  camera.resource_pool.Clear();
  camera.resources_dirty = true;
}

Ref<RenderPipeline> Scene::CreateDefaultPipeline(Ref<Renderer> renderer) {
  auto pipeline = CreateReference<RenderPipeline>(renderer);
  pipeline->AddFeature<ShadowFeature>(renderer);
  pipeline->AddFeature<GeometryFeature>(renderer);
  pipeline->AddFeature<SSAOFeature>(renderer);
  pipeline->AddFeature<LightingFeature>(renderer);
  pipeline->AddFeature<SpriteFeature>(renderer);
  pipeline->AddFeature<CompositeFeature>(renderer);
  pipeline->AddFeature<TAAFeature>(renderer);
  pipeline->AddFeature<BloomFeature>(renderer);
  pipeline->AddFeature<MotionBlurFeature>(renderer);
  pipeline->AddFeature<FXAAFeature>(renderer);
  return pipeline;
}

}  // namespace Wiesel