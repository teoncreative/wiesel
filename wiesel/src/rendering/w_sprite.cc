//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 24/04/2025.
//

#include "rendering/w_sprite.h"
#include "asset/w_asset_manager.h"
#include "rendering/w_sprite_asset.h"
#include "w_engine.h"

namespace Wiesel {

void SpriteAnimatorComponent::Play(const std::string& state_name,
                                   bool restart) {
  if (!restart && current_state_name_ == state_name && playing_) {
    return;
  }
  current_state_name_ = state_name;
  state_machine_.current_state = state_name;
  state_machine_.state_time = 0.0f;
  current_frame_index_ = 0;
  frame_timer_ = 0.0f;
  playing_ = true;

  // Resolve the animation for this state
  auto controller_data = Engine::asset_manager().Get<SpriteControllerAssetData>(
      controller_handle_);
  if (controller_data) {
    const auto* state = controller_data->FindState(state_name);
    if (state && state->animation_handle.IsValid()) {
      current_anim_ = Engine::asset_manager().Get<SpriteAnimAssetData>(
          state->animation_handle);
    }
  }
}

void SpriteAnimatorComponent::Stop() {
  playing_ = false;
}

}  // namespace Wiesel
