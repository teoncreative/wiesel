//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_sprite_animation_system.h"

#include "asset/w_asset_manager.h"
#include "rendering/w_sprite.h"
#include "rendering/w_sprite_asset.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace Wiesel {

void SpriteAnimationSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("SpriteAnimationSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  for (auto entity : registry.view<SpriteAnimatorComponent>()) {
    auto& animator = registry.get<SpriteAnimatorComponent>(entity);
    if (!registry.all_of<SpriteRendererComponent>(entity)) {
      continue;
    }
    auto& renderer_comp = registry.get<SpriteRendererComponent>(entity);

    if (!animator.controller_handle_.IsValid()) {
      continue;
    }

    // Load controller data if not yet resolved
    std::shared_ptr<SpriteControllerAssetData> controller_data =
        Engine::asset_manager().Get<SpriteControllerAssetData>(
            animator.controller_handle_);
    if (!controller_data) {
      continue;
    }

    // Initialize state machine controller if empty
    if (animator.state_machine_.controller.IsEmpty()) {
      animator.state_machine_.controller.default_state =
          controller_data->default_state;
      for (const auto& state : controller_data->states) {
        AnimationState anim_state;
        anim_state.name = state.name;
        anim_state.clip_name = state.name;  // map state name to itself
        anim_state.speed = state.speed;
        anim_state.looping = true;
        animator.state_machine_.controller.states.push_back(
            std::move(anim_state));
      }
      animator.state_machine_.controller.transitions =
          controller_data->transitions;
      animator.state_machine_.EnsureDefaultState();
      animator.current_state_name_ = animator.state_machine_.current_state;
    }

    // Evaluate state machine transitions
    std::string new_state = animator.state_machine_.EvaluateTransitions();
    if (!new_state.empty()) {
      animator.current_state_name_ = new_state;
      animator.current_frame_index_ = 0;
      animator.frame_timer_ = 0.0f;
      animator.current_anim_ = nullptr;  // force re-resolve
    }

    // Resolve current animation if needed
    if (!animator.current_anim_) {
      const auto* state =
          controller_data->FindState(animator.current_state_name_);
      if (state && state->animation_handle.IsValid()) {
        animator.current_anim_ =
            Engine::asset_manager().Get<SpriteAnimAssetData>(
                state->animation_handle);
      }
    }

    if (!animator.current_anim_ || animator.current_anim_->frames.empty()) {
      continue;
    }

    if (!animator.playing_) {
      // Still set the sprite even when paused
      const auto& frame =
          animator.current_anim_->frames[animator.current_frame_index_];
      if (renderer_comp.sprite_handle_ != frame.sprite_handle) {
        renderer_comp.sprite_handle_ = frame.sprite_handle;
      }
      continue;
    }

    // Get speed from controller state
    float speed = 1.0f;
    const auto* state =
        controller_data->FindState(animator.current_state_name_);
    if (state) {
      speed = state->speed;
    }

    // Advance frame timer
    const auto& frames = animator.current_anim_->frames;
    animator.frame_timer_ += delta_time * speed;
    float frame_duration = frames[animator.current_frame_index_].duration;
    if (frame_duration > 0.0f && animator.frame_timer_ >= frame_duration) {
      animator.frame_timer_ -= frame_duration;
      animator.current_frame_index_++;
      if (animator.current_frame_index_ >=
          static_cast<uint32_t>(frames.size())) {
        if (animator.current_anim_->loop) {
          animator.current_frame_index_ = 0;
        } else {
          animator.current_frame_index_ =
              static_cast<uint32_t>(frames.size()) - 1;
          animator.playing_ = false;
        }
      }
    }

    // Set the sprite on the renderer
    const auto& current_frame = frames[animator.current_frame_index_];
    if (renderer_comp.sprite_handle_ != current_frame.sprite_handle) {
      renderer_comp.sprite_handle_ = current_frame.sprite_handle;
    }
  }
}

}  // namespace Wiesel
