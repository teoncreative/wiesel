//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_skeletal_animation_system.h"

#include "animation/w_animation.h"
#include "animation/w_animation_controller.h"
#include "animation/w_animator.h"
#include "asset/w_asset_manager.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace Wiesel {

void SkeletalAnimationSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("SkeletalAnimationSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  for (const auto& entity :
       registry.view<AnimatorComponent, ModelComponent>()) {
    auto& animator = registry.get<AnimatorComponent>(entity);
    auto& model_comp = registry.get<ModelComponent>(entity);
    if (!animator.playing) {
      continue;
    }

    const std::shared_ptr<Model>& model_data =
        Engine::asset_manager().GetOrLoad<Model>(model_comp.model_handle);
    if (!model_data || model_data->animation_clips.empty()) {
      continue;
    }

    // Helper: find clip by name in model
    auto find_clip = [&](const std::string& name) -> const AnimationClip* {
      for (const auto& c : model_data->animation_clips) {
        if (c.name == name) {
          return &c;
        }
      }
      return nullptr;
    };

    if (animator.UseController()) {
      // --- Controller mode ---
      AnimationController& ctrl = animator.controller;

      // Initialize state if needed
      if (animator.current_state_name.empty() && !ctrl.default_state.empty()) {
        animator.current_state_name = ctrl.default_state;
        animator.state_time = 0.0f;
      }

      const auto* cur_state = ctrl.FindState(animator.current_state_name);
      if (!cur_state) {
        continue;
      }

      // Check transitions (current state transitions first, then "any state")
      const AnimationTransition* fired_transition = nullptr;
      for (const AnimationTransition& trans : ctrl.transitions) {
        if (!trans.from_state.empty() &&
            trans.from_state != animator.current_state_name) {
          continue;
        }
        // Evaluate all conditions
        bool all_met = true;
        for (const TransitionCondition& cond : trans.conditions) {
          auto param_it = animator.parameters.find(cond.param_name);
          if (param_it == animator.parameters.end()) {
            all_met = false;
            break;
          }
          const auto& param = param_it->second;
          switch (cond.param_type) {
            case AnimParamType::Trigger:
              if (!param.b) {
                all_met = false;
              }
              break;
            case AnimParamType::Bool:
              switch (cond.op) {
                case ConditionOp::Equals:
                  if (param.b != cond.value.b) {
                    all_met = false;
                  }
                  break;
                case ConditionOp::NotEquals:
                  if (param.b == cond.value.b) {
                    all_met = false;
                  }
                  break;
                default:
                  break;
              }
              break;
            case AnimParamType::Int:
              switch (cond.op) {
                case ConditionOp::Equals:
                  if (param.i != cond.value.i) {
                    all_met = false;
                  }
                  break;
                case ConditionOp::NotEquals:
                  if (param.i == cond.value.i) {
                    all_met = false;
                  }
                  break;
                case ConditionOp::Greater:
                  if (param.i <= cond.value.i) {
                    all_met = false;
                  }
                  break;
                case ConditionOp::Less:
                  if (param.i >= cond.value.i) {
                    all_met = false;
                  }
                  break;
              }
              break;
            case AnimParamType::Float:
              switch (cond.op) {
                case ConditionOp::Equals:
                  if (std::abs(param.f - cond.value.f) > 0.001f) {
                    all_met = false;
                  }
                  break;
                case ConditionOp::NotEquals:
                  if (std::abs(param.f - cond.value.f) <= 0.001f) {
                    all_met = false;
                  }
                  break;
                case ConditionOp::Greater:
                  if (param.f <= cond.value.f) {
                    all_met = false;
                  }
                  break;
                case ConditionOp::Less:
                  if (param.f >= cond.value.f) {
                    all_met = false;
                  }
                  break;
              }
              break;
          }
          if (!all_met) {
            break;
          }
        }
        if (all_met && trans.to_state != animator.current_state_name) {
          fired_transition = &trans;
          break;
        }
      }

      if (fired_transition) {
        // Consume triggers used in this transition
        for (const TransitionCondition& cond : fired_transition->conditions) {
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
        if (!cur_state) {
          continue;
        }
      }

      // Find the current clip
      const AnimationClip* clip = find_clip(cur_state->clip_name);
      if (!clip) {
        continue;
      }

      // Advance state time
      float tps = clip->ticks_per_second;
      animator.state_time +=
          delta_time * cur_state->speed * animator.playback_speed * tps;
      if (cur_state->looping && clip->duration > 0.0f) {
        animator.state_time = std::fmod(animator.state_time, clip->duration);
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
            if (animator.prev_clip_time < 0.0f) {
              animator.prev_clip_time += prev_clip->duration;
            }
          }

          Animator::Evaluate(*model_data, *prev_clip, animator.prev_clip_time,
                             animator.prev_bone_matrices,
                             animator.prev_node_transforms);
        }

        // Blend node transforms, then recompute bone matrices
        Animator::BlendAndSkin(*model_data, animator.prev_node_transforms,
                               animator.node_transforms, animator.blend_weight,
                               animator.bone_matrices,
                               animator.node_transforms);

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
        if (!ovr.enabled) {
          continue;
        }

        // Resolve indices on first use
        if (ovr.cached_node_index < 0) {
          ovr.cached_node_index = hierarchy.FindNode(ovr.bone_name);
          ovr.cached_bone_index = skeleton.FindBone(ovr.bone_name);
        }
        int ni = ovr.cached_node_index;
        int bi = ovr.cached_bone_index;
        if (ni < 0 || bi < 0 ||
            ni >= static_cast<int>(animator.node_transforms.size()) ||
            bi >= static_cast<int>(animator.bone_matrices.size())) {
          continue;
        }

        // Decompose current global node transform
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(animator.node_transforms[ni], scale, rot, pos, skew,
                       persp);

        // Apply additional rotation in local space
        glm::quat new_rot = rot * ovr.additional_rotation;
        animator.node_transforms[ni] = glm::translate(glm::mat4(1.0f), pos) *
                                       glm::mat4_cast(new_rot) *
                                       glm::scale(glm::mat4(1.0f), scale);

        // Recompute bone matrix for this bone
        animator.bone_matrices[bi] = animator.node_transforms[ni] *
                                     skeleton.bones[bi].inverse_bind_matrix;

        // Re-propagate to children
        const auto& node = hierarchy.nodes[ni];
        for (int32_t child_idx : node.children) {
          if (child_idx < 0 ||
              child_idx >= static_cast<int>(animator.node_transforms.size())) {
            continue;
          }

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
}

}  // namespace Wiesel
