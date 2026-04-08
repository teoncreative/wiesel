//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "animation/w_animation_controller.h"
#include "asset/w_asset_handle.h"
#include "w_pch.h"

namespace Wiesel {

// Runtime data for a .wanimcontroller asset - a state machine that references
// animation clips by AssetHandle. The controller is clip-format-agnostic:
// it doesn't know or care whether clips are skeletal or sprite.
struct AnimControllerAssetData {
  struct State {
    std::string name;
    AssetHandle clip_handle;  // -> .wanimclip
    float speed = 1.0f;
    // Editor layout (persisted in asset, not used at runtime)
    glm::vec2 editor_pos = {0.0f, 0.0f};
    int32_t editor_id = -1;
  };

  std::string default_state;
  std::vector<State> states;
  std::vector<AnimationTransition> transitions;
  std::map<std::string, AnimParam> default_parameters;

  const State* FindState(const std::string& name) const {
    for (const auto& s : states) {
      if (s.name == name) {
        return &s;
      }
    }
    return nullptr;
  }

  bool IsEmpty() const { return states.empty(); }
};

}  // namespace Wiesel