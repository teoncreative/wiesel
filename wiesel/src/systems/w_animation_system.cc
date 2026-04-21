//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_animation_system.h"

#include "animation/w_animation.h"
#include "animation/w_animation_clip_asset.h"
#include "animation/w_animation_controller_asset.h"
#include "animation/w_animator.h"
#include "animation/w_property_curve_eval.h"
#include "asset/w_asset_manager.h"
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_renderer.h"
#include "rendering/w_sprite.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace wiesel {

void AnimationSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("AnimationSystem::Update");
  entt::registry& registry = scene.GetRegistry();

  // --- Pass 1: Common (all AnimatorComponent entities) ---
  for (auto entity : registry.view<AnimatorComponent>()) {
    auto& animator = registry.get<AnimatorComponent>(entity);
    if (!animator.playing || !animator.controller_handle.IsValid()) {
      continue;
    }

    // Emplace runtime components if missing
    bool has_sprite = registry.all_of<SpriteRendererComponent>(entity);

    // Ensure SkeletalAnimRuntime exists for new-style hierarchy roots
    if (!registry.all_of<SkeletalAnimRuntime>(entity)) {
      for (auto smr_entity : registry.view<SkinnedMeshRendererComponent>()) {
        auto& smr = registry.get<SkinnedMeshRendererComponent>(smr_entity);
        if (smr.skeleton_root == entity) {
          auto& skel = registry.emplace<SkeletalAnimRuntime>(entity);
          skel.model_handle = smr.model_handle;
          skel.Initialize();
          break;
        }
      }
    }

    if (has_sprite && !registry.all_of<SpriteAnimRuntime>(entity)) {
      registry.emplace<SpriteAnimRuntime>(entity);
    }

    // Load controller asset and init state machine
    auto controller_data = Engine::asset_manager().Get<AnimControllerAssetData>(
        animator.controller_handle);
    if (!controller_data || controller_data->IsEmpty()) {
      continue;
    }

    if (animator.state_machine.controller.IsEmpty()) {
      // Build state machine from controller asset
      animator.state_machine.controller.default_state =
          controller_data->default_state;
      for (const auto& state : controller_data->states) {
        AnimationState anim_state;
        anim_state.name = state.name;
        anim_state.clip_name = state.name;  // state machine uses name as key
        anim_state.speed = state.speed;
        anim_state.looping = true;
        animator.state_machine.controller.states.push_back(
            std::move(anim_state));
      }
      animator.state_machine.controller.transitions =
          controller_data->transitions;
      // Copy default parameters
      animator.state_machine.parameters.insert(
          controller_data->default_parameters.begin(),
          controller_data->default_parameters.end());
      animator.state_machine.EnsureDefaultState();
    }

    // Evaluate transitions
    std::string prev_state = animator.state_machine.current_state;
    std::string new_state = animator.state_machine.EvaluateTransitions();
    bool state_changed = !new_state.empty();

    // Resolve current state's clip
    const auto* current_state =
        controller_data->FindState(animator.state_machine.current_state);
    if (!current_state || !current_state->clip_handle.IsValid()) {
      continue;
    }

    auto clip_data = Engine::asset_manager().Get<AnimClipAssetData>(
        current_state->clip_handle);
    if (!clip_data) {
      // Try loading synchronously
      Engine::asset_manager().LoadSync(current_state->clip_handle);
      clip_data = Engine::asset_manager().Get<AnimClipAssetData>(
          current_state->clip_handle);
      if (!clip_data) {
        continue;
      }
    }

    // --- Pass 2: Skeletal ---
    bool has_skel_runtime = registry.all_of<SkeletalAnimRuntime>(entity);
    if (has_skel_runtime && clip_data->HasBoneChannels()) {
      auto& skel = registry.get<SkeletalAnimRuntime>(entity);

      AssetHandle skel_model_handle = skel.model_handle;

      const auto& model_data =
          Engine::asset_manager().GetOrStartLoad<Model>(skel_model_handle);
      if (!model_data) {
        continue;
      }

      // Handle transition -> crossfade setup
      if (state_changed) {
        // Find the transition to get blend_duration
        float blend_dur = 0.0f;
        for (const auto& trans : controller_data->transitions) {
          if (trans.to_state == new_state &&
              (trans.from_state.empty() || trans.from_state == prev_state)) {
            blend_dur = trans.blend_duration;
            break;
          }
        }

        if (blend_dur > 0.0f && !skel.bone_matrices.empty()) {
          skel.prev_bone_matrices = skel.bone_matrices;
          skel.prev_node_transforms = skel.node_transforms;
          skel.prev_clip_time = animator.state_machine.state_time;
          skel.is_blending = true;
          skel.blend_duration = blend_dur;
          skel.blend_elapsed = 0.0f;
          skel.blend_weight = 0.0f;
        } else {
          skel.is_blending = false;
        }
      }

      // Advance state time
      float tps = clip_data->ticks_per_second;
      animator.state_machine.state_time +=
          delta_time * current_state->speed * animator.playback_speed * tps;
      if (clip_data->loop && clip_data->duration > 0.0f) {
        animator.state_machine.state_time =
            std::fmod(animator.state_machine.state_time, clip_data->duration);
        if (animator.state_machine.state_time < 0.0f) {
          animator.state_machine.state_time += clip_data->duration;
        }
      } else {
        animator.state_machine.state_time = glm::clamp(
            animator.state_machine.state_time, 0.0f, clip_data->duration);
      }

      // Build a temporary AnimationClip for Animator::Evaluate
      AnimationClip temp_clip;
      temp_clip.name = current_state->name;
      temp_clip.duration = clip_data->duration;
      temp_clip.ticks_per_second = clip_data->ticks_per_second;
      temp_clip.channels = clip_data->bone_channels;

      skel.max_bone_reach = clip_data->max_bone_reach;

      Animator::Evaluate(*model_data, temp_clip,
                         animator.state_machine.state_time, skel.bone_matrices,
                         skel.node_transforms);

      // Crossfade blending
      if (skel.is_blending) {
        skel.blend_elapsed += delta_time;
        skel.blend_weight =
            glm::clamp(skel.blend_elapsed / skel.blend_duration, 0.0f, 1.0f);

        // Blend node transforms and recompute bone matrices
        Animator::BlendAndSkin(*model_data, skel.prev_node_transforms,
                               skel.node_transforms, skel.blend_weight,
                               skel.bone_matrices, skel.node_transforms);

        if (skel.blend_weight >= 1.0f) {
          skel.is_blending = false;
        }
      }

      // Bone overrides
      if (!skel.bone_overrides.empty()) {
        const auto& hierarchy = model_data->node_hierarchy;
        const auto& skeleton = model_data->skeleton;

        for (auto& ovr : skel.bone_overrides) {
          if (!ovr.enabled) {
            continue;
          }

          if (ovr.cached_node_index < 0) {
            ovr.cached_node_index = hierarchy.FindNode(ovr.bone_name);
            ovr.cached_bone_index = skeleton.FindBone(ovr.bone_name);
          }
          int ni = ovr.cached_node_index;
          int bi = ovr.cached_bone_index;
          if (ni < 0 || bi < 0 ||
              ni >= static_cast<int>(skel.node_transforms.size()) ||
              bi >= static_cast<int>(skel.bone_matrices.size())) {
            continue;
          }

          glm::vec3 pos, scale, skew;
          glm::quat rot;
          glm::vec4 persp;
          glm::decompose(skel.node_transforms[ni], scale, rot, pos, skew,
                         persp);

          glm::quat new_rot = rot * ovr.additional_rotation;
          skel.node_transforms[ni] = glm::translate(glm::mat4(1.0f), pos) *
                                     glm::mat4_cast(new_rot) *
                                     glm::scale(glm::mat4(1.0f), scale);

          skel.bone_matrices[bi] =
              skel.node_transforms[ni] * skeleton.bones[bi].inverse_bind_matrix;

          const auto& node = hierarchy.nodes[ni];
          for (int32_t child_idx : node.children) {
            if (child_idx < 0 ||
                child_idx >= static_cast<int>(skel.node_transforms.size())) {
              continue;
            }

            skel.node_transforms[child_idx] =
                skel.node_transforms[ni] *
                hierarchy.nodes[child_idx].local_transform;

            int32_t child_bone = hierarchy.nodes[child_idx].bone_index;
            if (child_bone >= 0 &&
                child_bone < static_cast<int>(skel.bone_matrices.size())) {
              skel.bone_matrices[child_bone] =
                  skel.node_transforms[child_idx] *
                  skeleton.bones[child_bone].inverse_bind_matrix;
            }
          }
        }
      }

      // Upload bone matrices to GPU
      std::shared_ptr<UniformBuffer> bone_ubo = skel.bone_ubo;
      if (bone_ubo && bone_ubo->data_) {
        BoneMatricesUniformData gpu_data{};
        size_t count = std::min(skel.bone_matrices.size(),
                                static_cast<size_t>(WIESEL_MAX_BONES));
        for (size_t b = 0; b < count; b++) {
          gpu_data.bone_matrices[b] = skel.bone_matrices[b];
        }
        memcpy(bone_ubo->data_, &gpu_data, sizeof(BoneMatricesUniformData));
      }
    }

    // --- Pass 3: Property curves (any WPROPERTY(Animatable) field) ---
    if (clip_data->HasPropertyCurves()) {
      // Advance property curve time (separate from skeletal state_time
      // because property curves may exist without bone channels)
      float prop_time = animator.state_machine.state_time;
      if (!clip_data->HasBoneChannels()) {
        // No bone channels - advance time here (skeletal pass didn't run)
        float tps = clip_data->ticks_per_second;
        animator.state_machine.state_time +=
            delta_time * current_state->speed * animator.playback_speed * tps;
        if (clip_data->loop && clip_data->duration > 0.0f) {
          animator.state_machine.state_time =
              std::fmod(animator.state_machine.state_time, clip_data->duration);
          if (animator.state_machine.state_time < 0.0f) {
            animator.state_machine.state_time += clip_data->duration;
          }
        } else {
          animator.state_machine.state_time = glm::clamp(
              animator.state_machine.state_time, 0.0f, clip_data->duration);
          if (animator.state_machine.state_time >= clip_data->duration &&
              !clip_data->loop) {
            animator.playing = false;
          }
        }
        prop_time = animator.state_machine.state_time;
      }

      EvaluatePropertyCurves(scene, entity, clip_data->property_curves,
                             prop_time);
    }
  }
}

}  // namespace wiesel