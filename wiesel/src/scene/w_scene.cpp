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
#include "rendering/features/w_canvas_feature.hpp"
#include "rendering/features/w_fxaa_feature.hpp"
#include "rendering/features/w_rt_shadow_feature.hpp"
#include "animation/w_animation.hpp"
#include "animation/w_animation_controller.hpp"
#include "animation/w_animator.hpp"
#include "asset/w_asset_manager.hpp"
#include "scene/w_entity.hpp"
#include "systems/w_canvas_system.hpp"
#include "ai/w_agent_controller.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "w_engine.hpp"

namespace Wiesel {
class PipelineRecreatedEvent;

Scene::Scene() {
  current_camera_ = CreateReference<CameraData>();
  physics_world_ = std::make_unique<PhysicsWorld>(this);
}

Scene::~Scene() {
  LOG_DEBUG("~Scene destructor");
  Cleanup();
}

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

entt::entity Scene::FindEntityByName(const std::string& name) {
  for (auto entity : registry_.view<TagComponent>()) {
    if (registry_.get<TagComponent>(entity).tag == name) {
      return entity;
    }
  }
  return entt::null;
}

void Scene::DestroyEntity(entt::entity handle) {
  registry_.destroy(handle);
}

void Scene::OnUpdate(float_t delta_time) {
  PROFILE_ZONE_SCOPED();
  if (!first_update_) [[likely]] {
    // Create bodies for new entities before scripts run
    physics_world_->EnsureBodiesExist();

    {
      PROFILE_ZONE_SCOPED_N("Behaviors::OnUpdate");
      for (const auto& entity : registry_.view<BehaviorsComponent>()) {
        BehaviorsComponent& component = registry_.get<BehaviorsComponent>(entity);
        for (IBehavior*& value : component.behaviors_ | std::views::values) {
          value->OnUpdate(delta_time);
        }
      }
    }
    {
      PROFILE_ZONE_SCOPED_N("AgentController::Evaluate");
      for (const auto& entity : registry_.view<AgentController>()) {
        auto& controller = registry_.get<AgentController>(entity);
        controller.Evaluate(delta_time);
        controller.Update(delta_time);
      }
    }
    {
      PROFILE_ZONE_SCOPED_N("Systems::Update");
      for (auto&& fn : systems_[SystemType::Update]) {
        fn(delta_time);
      }
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
  PROFILE_ZONE_SCOPED_N("Scene::UpdateSceneState");
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
  auto& lights = Engine::renderer()->lights_uniform_data_;
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

  // Animation evaluation
  {
  PROFILE_ZONE_SCOPED_N("Animation Evaluation");
  for (const auto& entity :
       registry_.view<AnimatorComponent, ModelComponent>()) {
    auto& animator = registry_.get<AnimatorComponent>(entity);
    auto& model_comp = registry_.get<ModelComponent>(entity);
    if (!animator.playing) {
      continue;
    }

    const Ref<Model>& model_data =
        Engine::asset_manager().GetOrLoad<Model>(model_comp.model_handle);
    if (!model_data || model_data->animation_clips.empty()) {
      continue;
    }

    // Helper: find clip by name in model
    auto find_clip = [&](const std::string& name) -> const AnimationClip* {
      for (const auto& c : model_data->animation_clips) {
        if (c.name == name) return &c;
      }
      return nullptr;
    };

    if (animator.UseController()) {
      // --- Controller mode ---
      auto& ctrl = animator.controller;

      // Initialize state if needed
      if (animator.current_state_name.empty() && !ctrl.default_state.empty()) {
        animator.current_state_name = ctrl.default_state;
        animator.state_time = 0.0f;
      }

      const auto* cur_state = ctrl.FindState(animator.current_state_name);
      if (!cur_state) continue;

      // Check transitions (current state transitions first, then "any state")
      const AnimationTransition* fired_transition = nullptr;
      for (const auto& trans : ctrl.transitions) {
        if (!trans.from_state.empty() &&
            trans.from_state != animator.current_state_name)
          continue;
        // Evaluate all conditions
        bool all_met = true;
        for (const auto& cond : trans.conditions) {
          auto param_it = animator.parameters.find(cond.param_name);
          if (param_it == animator.parameters.end()) {
            all_met = false;
            break;
          }
          const auto& param = param_it->second;
          switch (cond.param_type) {
            case AnimParamType::Trigger:
              if (!param.b) all_met = false;
              break;
            case AnimParamType::Bool:
              switch (cond.op) {
                case ConditionOp::Equals:
                  if (param.b != cond.value.b) all_met = false;
                  break;
                case ConditionOp::NotEquals:
                  if (param.b == cond.value.b) all_met = false;
                  break;
                default:
                  break;
              }
              break;
            case AnimParamType::Int:
              switch (cond.op) {
                case ConditionOp::Equals:
                  if (param.i != cond.value.i) all_met = false;
                  break;
                case ConditionOp::NotEquals:
                  if (param.i == cond.value.i) all_met = false;
                  break;
                case ConditionOp::Greater:
                  if (param.i <= cond.value.i) all_met = false;
                  break;
                case ConditionOp::Less:
                  if (param.i >= cond.value.i) all_met = false;
                  break;
              }
              break;
            case AnimParamType::Float:
              switch (cond.op) {
                case ConditionOp::Equals:
                  if (std::abs(param.f - cond.value.f) > 0.001f)
                    all_met = false;
                  break;
                case ConditionOp::NotEquals:
                  if (std::abs(param.f - cond.value.f) <= 0.001f)
                    all_met = false;
                  break;
                case ConditionOp::Greater:
                  if (param.f <= cond.value.f) all_met = false;
                  break;
                case ConditionOp::Less:
                  if (param.f >= cond.value.f) all_met = false;
                  break;
              }
              break;
          }
          if (!all_met) break;
        }
        if (all_met && trans.to_state != animator.current_state_name) {
          fired_transition = &trans;
          break;
        }
      }

      if (fired_transition) {
        // Consume triggers used in this transition
        for (const auto& cond : fired_transition->conditions) {
          if (cond.param_type == AnimParamType::Trigger) {
            auto param_it = animator.parameters.find(cond.param_name);
            if (param_it != animator.parameters.end()) {
              param_it->second.b = false;
            }
          }
        }

        // Start crossfade
        if (fired_transition->blend_duration > 0.0f) {
          animator.prev_clip_name = cur_state->clip_name;
          animator.prev_clip_time = animator.state_time;
          animator.prev_bone_matrices = animator.bone_matrices;
          animator.prev_node_transforms = animator.node_transforms;
          animator.is_blending = true;
          animator.blend_duration = fired_transition->blend_duration;
          animator.blend_elapsed = 0.0f;
          animator.blend_weight = 0.0f;
        } else {
          animator.is_blending = false;
        }

        animator.current_state_name = fired_transition->to_state;
        animator.state_time = 0.0f;
        cur_state = ctrl.FindState(animator.current_state_name);
        if (!cur_state) continue;
      }

      // Find the current clip
      const AnimationClip* clip = find_clip(cur_state->clip_name);
      if (!clip) continue;

      // Advance state time
      float tps = clip->ticks_per_second;
      animator.state_time +=
          delta_time * cur_state->speed * animator.playback_speed * tps;
      if (cur_state->looping && clip->duration > 0.0f) {
        animator.state_time =
            std::fmod(animator.state_time, clip->duration);
        if (animator.state_time < 0.0f) {
          animator.state_time += clip->duration;
        }
      } else {
        animator.state_time =
            glm::clamp(animator.state_time, 0.0f, clip->duration);
      }

      // Evaluate current clip
      Animator::Evaluate(*model_data, *clip, animator.state_time,
                         animator.bone_matrices, animator.node_transforms);

      // Handle crossfade blending
      if (animator.is_blending) {
        animator.blend_elapsed += delta_time;
        animator.blend_weight = glm::clamp(
            animator.blend_elapsed / animator.blend_duration, 0.0f, 1.0f);

        // Advance previous clip time too (so it doesn't freeze)
        const AnimationClip* prev_clip = find_clip(animator.prev_clip_name);
        if (prev_clip) {
          // prev_clip_time was captured at transition start; keep advancing it
          float prev_tps = prev_clip->ticks_per_second;
          animator.prev_clip_time += delta_time * prev_tps;
          if (prev_clip->duration > 0.0f) {
            animator.prev_clip_time =
                std::fmod(animator.prev_clip_time, prev_clip->duration);
            if (animator.prev_clip_time < 0.0f)
              animator.prev_clip_time += prev_clip->duration;
          }

          Animator::Evaluate(*model_data, *prev_clip, animator.prev_clip_time,
                             animator.prev_bone_matrices,
                             animator.prev_node_transforms);
        }

        // Blend node transforms, then recompute bone matrices
        Animator::BlendAndSkin(*model_data,
                               animator.prev_node_transforms,
                               animator.node_transforms, animator.blend_weight,
                               animator.bone_matrices, animator.node_transforms);

        if (animator.blend_weight >= 1.0f) {
          animator.is_blending = false;
        }
      }

      // Keep legacy fields in sync for editor display
      animator.current_clip_name = cur_state->clip_name;
      animator.playback_time = animator.state_time;
      animator.looping = cur_state->looping;

    } else {
      // --- Legacy single-clip mode ---
      const AnimationClip* clip = nullptr;
      if (!animator.current_clip_name.empty()) {
        clip = find_clip(animator.current_clip_name);
      }
      if (!clip) {
        clip = &model_data->animation_clips[0];
        animator.current_clip_name = clip->name;
      }

      float tps = clip->ticks_per_second;
      animator.playback_time += delta_time * animator.playback_speed * tps;
      if (animator.looping && clip->duration > 0.0f) {
        animator.playback_time =
            std::fmod(animator.playback_time, clip->duration);
        if (animator.playback_time < 0.0f) {
          animator.playback_time += clip->duration;
        }
      } else {
        animator.playback_time =
            glm::clamp(animator.playback_time, 0.0f, clip->duration);
      }

      Animator::Evaluate(*model_data, *clip, animator.playback_time,
                         animator.bone_matrices, animator.node_transforms);
    }

    // Apply bone overrides (e.g. head look-at)
    if (!animator.bone_overrides.empty() && model_data) {
      const auto& hierarchy = model_data->node_hierarchy;
      const auto& skeleton = model_data->skeleton;

      for (auto& ovr : animator.bone_overrides) {
        if (!ovr.enabled) continue;

        // Resolve indices on first use
        if (ovr.cached_node_index < 0) {
          ovr.cached_node_index = hierarchy.FindNode(ovr.bone_name);
          ovr.cached_bone_index = skeleton.FindBone(ovr.bone_name);
        }
        int ni = ovr.cached_node_index;
        int bi = ovr.cached_bone_index;
        if (ni < 0 || bi < 0 ||
            ni >= static_cast<int>(animator.node_transforms.size()) ||
            bi >= static_cast<int>(animator.bone_matrices.size()))
          continue;

        // Decompose current global node transform
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(animator.node_transforms[ni], scale, rot, pos, skew,
                        persp);

        // Apply additional rotation in local space
        glm::quat new_rot = rot * ovr.additional_rotation;
        animator.node_transforms[ni] =
            glm::translate(glm::mat4(1.0f), pos) *
            glm::mat4_cast(new_rot) *
            glm::scale(glm::mat4(1.0f), scale);

        // Recompute bone matrix for this bone
        animator.bone_matrices[bi] =
            animator.node_transforms[ni] *
            skeleton.bones[bi].inverse_bind_matrix;

        // Re-propagate to children
        const auto& node = hierarchy.nodes[ni];
        for (int32_t child_idx : node.children) {
          if (child_idx < 0 ||
              child_idx >= static_cast<int>(animator.node_transforms.size()))
            continue;

          animator.node_transforms[child_idx] =
              animator.node_transforms[ni] *
              hierarchy.nodes[child_idx].local_transform;

          int32_t child_bone = hierarchy.nodes[child_idx].bone_index;
          if (child_bone >= 0 &&
              child_bone < static_cast<int>(animator.bone_matrices.size())) {
            animator.bone_matrices[child_bone] =
                animator.node_transforms[child_idx] *
                skeleton.bones[child_bone].inverse_bind_matrix;
          }
        }
      }
    }

    // Upload bone matrices to GPU
    if (model_comp.bone_ubo_ && model_comp.bone_ubo_->data_) {
      BoneMatricesUniformData gpu_data{};
      size_t count = std::min(animator.bone_matrices.size(),
                              static_cast<size_t>(WIESEL_MAX_BONES));
      for (size_t b = 0; b < count; b++) {
        gpu_data.bone_matrices[b] = animator.bone_matrices[b];
      }
      memcpy(model_comp.bone_ubo_->data_, &gpu_data,
             sizeof(BoneMatricesUniformData));
    }
  }
  }  // end Animation Evaluation profile zone

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
  PROFILE_ZONE_SCOPED_N("Scene::OnEvent");
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
  if (render_resolution_.x > 0 && render_resolution_.y > 0) {
    return false;  // fixed resolution, don't react to window resize
  }
  for (const auto& entity : registry_.view<CameraComponent>()) {
    auto& component = registry_.get<CameraComponent>(entity);
    component.viewport_size.x = event.window_size().width;
    component.viewport_size.y = event.window_size().height;
    component.aspect_ratio = event.aspect_ratio();
    component.resources_dirty = true;
    component.view_changed = true;
  }
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

void Scene::Cleanup() {
  LOG_DEBUG("Scene::Cleanup - render_graphs: {}, external: {}",
            render_graphs_.size(), external_render_graph_ != nullptr);
  render_graphs_.clear();
  external_render_graph_ = nullptr;
  default_pipeline_ = nullptr;

  // Clear camera resource pools so their descriptor sets are freed.
  auto camera_view = registry_.view<CameraComponent>();
  LOG_DEBUG("Scene::Cleanup - cameras: {}", camera_view.size());
  for (auto entity : camera_view) {
    auto& camera = registry_.get<CameraComponent>(entity);
    camera.resource_pool.Clear();
    camera.render_pipeline = nullptr;
  }

  // Clear per-entity render data (descriptor sets, uniform buffers).
  for (auto entity : registry_.view<ModelComponent>()) {
    auto& model = registry_.get<ModelComponent>(entity);
    model.geometry_descriptors.clear();
    model.shadow_descriptors.clear();
    model.uniform_buffer = nullptr;
    model.bone_ubo_ = nullptr;
    model.bone_descriptor_ = nullptr;
    model.mesh_uniform_buffers_.clear();
  }

  for (auto entity : registry_.view<CanvasRectComponent>()) {
    auto& rect = registry_.get<CanvasRectComponent>(entity);
    rect.descriptor_ = nullptr;
    rect.ubo_ = nullptr;
  }

  for (auto entity : registry_.view<CanvasImageComponent>()) {
    auto& img = registry_.get<CanvasImageComponent>(entity);
    img.descriptor_ = nullptr;
    img.ubo_ = nullptr;
  }

  for (auto entity : registry_.view<TextComponent>()) {
    auto& text = registry_.get<TextComponent>(entity);
    text.glyph_gpu_.clear();
  }

  skybox_ = nullptr;
  current_camera_ = nullptr;
}

void Scene::BuildRenderGraph(entt::entity camera_entity) {
  PROFILE_ZONE_SCOPED_N("Scene::BuildRenderGraph");
  Ref<Renderer> renderer = Engine::renderer();
  auto& camera = registry_.get<CameraComponent>(camera_entity);

  std::shared_ptr<RenderGraph>& graph = render_graphs_[camera_entity];
  if (!graph) {
    graph = CreateReference<RenderGraph>(*renderer);
  } else {
    graph->Clear();
  }

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
  Ref<Renderer> renderer = Engine::renderer();

  // Ensure we have a default pipeline
  if (!default_pipeline_) {
    default_pipeline_ = CreateDefaultPipeline(renderer);
  }

  for (const auto& cameraEntity : GetAllEntitiesWith<CameraComponent>()) {
    auto& camera = registry_.get<CameraComponent>(cameraEntity);
    auto& camera_transform = registry_.get<TransformComponent>(cameraEntity);
    if (!camera.enabled)
      continue;

    // Apply scene render resolution if set
    if (render_resolution_.x > 0 && render_resolution_.y > 0) {
      if (render_resolution_.x != camera.viewport_size.x ||
          render_resolution_.y != camera.viewport_size.y) {
        camera.viewport_size = render_resolution_;
        camera.aspect_ratio = render_resolution_.x / render_resolution_.y;
        camera.view_changed = true;
        camera.resources_dirty = true;
      }
    }

    // Compute canvas layout using the camera's viewport size so the layout
    // matches the push constant the canvas shader receives.
    {
      CanvasSystem canvas_system;
      canvas_system.Update(*this, camera.viewport_size);
    }

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

    PROFILE_PLOT("Game Width", static_cast<double>(camera.viewport_size.x));
    PROFILE_PLOT("Game Height", static_cast<double>(camera.viewport_size.y));

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
  Ref<Renderer> renderer = Engine::renderer();

  if (!default_pipeline_) {
    default_pipeline_ = CreateDefaultPipeline(renderer);
  }

  // Apply scene render resolution if set
  if (render_resolution_.x > 0 && render_resolution_.y > 0) {
    if (render_resolution_.x != camera.viewport_size.x ||
        render_resolution_.y != camera.viewport_size.y) {
      camera.viewport_size = render_resolution_;
      camera.aspect_ratio = render_resolution_.x / render_resolution_.y;
      camera.view_changed = true;
      camera.resources_dirty = true;
    }
  }

  // Compute canvas layout using camera viewport (must match shader push constant)
  {
    CanvasSystem canvas_system;
    canvas_system.Update(*this, camera.viewport_size);
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

  // Compute shadow cascades for external camera (same as ECS cameras)
  auto& lights = Engine::renderer()->lights_uniform_data_;
  if (lights.direct_light_count > 0) {
    camera.ComputeCascades(glm::normalize(lights.direct_lights[0].direction));
  } else {
    camera.does_shadow_pass = false;
  }

  // Setup resources if dirty
  if (camera.resources_dirty) {
    vkDeviceWaitIdle(renderer->GetLogicalDevice());
    bool use_resolve = renderer->options().msaa_mode > SamplingMode::DISABLED;
    RenderContext ctx{*renderer, *this, camera, camera.resource_pool,
                      camera.viewport_size, use_resolve, true};
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
  if (!external_render_graph_) {
    external_render_graph_ = CreateReference<RenderGraph>(*renderer);
  } else {
    external_render_graph_->Clear();
  }
  bool use_resolve = renderer->options().msaa_mode > SamplingMode::DISABLED;
  RenderContext ctx{*renderer, *this, camera, camera.resource_pool,
                    camera.viewport_size, use_resolve, true};
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
  if (renderer->IsRayTracingSupported()) {
    pipeline->AddFeature<RTShadowFeature>(renderer);
  }
  pipeline->AddFeature<SSAOFeature>(renderer);
  pipeline->AddFeature<LightingFeature>(renderer);
  pipeline->AddFeature<SpriteFeature>(renderer);
  pipeline->AddFeature<CompositeFeature>(renderer);
  pipeline->AddFeature<TAAFeature>(renderer);
  pipeline->AddFeature<BloomFeature>(renderer);
  pipeline->AddFeature<MotionBlurFeature>(renderer);
  pipeline->AddFeature<FXAAFeature>(renderer);
  pipeline->AddFeature<CanvasFeature>(renderer);
  return pipeline;
}

}  // namespace Wiesel