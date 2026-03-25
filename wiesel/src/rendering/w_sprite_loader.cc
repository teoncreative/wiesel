//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_sprite_loader.h"

#include <nlohmann/json.hpp>

#include "animation/w_animation_controller.h"
#include "asset/w_asset_manager.h"
#include "rendering/w_sprite_asset.h"
#include "util/w_logger.h"
#include "util/w_vfs.h"
#include "w_engine.h"

namespace Wiesel {

static nlohmann::json LoadJsonAsset(const AssetHandle& handle) {
  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta) {
    return {};
  }

  VfsFile file = Engine::vfs()->Open(meta->virtual_source_path);
  if (!file) {
    return {};
  }

  try {
    std::string content((std::istreambuf_iterator<char>(file.Stream())),
                        std::istreambuf_iterator<char>());
    return nlohmann::json::parse(content);
  } catch (const std::exception& e) {
    LOG_ERROR("Failed to parse asset JSON: {}", e.what());
    return {};
  }
}

bool LoadSpriteAnimAsset(const AssetHandle& handle) {
  auto j = LoadJsonAsset(handle);
  if (j.is_null()) {
    return false;
  }

  if (!j.contains("frames") || !j["frames"].is_array()) {
    LOG_ERROR("SpriteAnim: missing 'frames' array");
    return false;
  }

  auto data = std::make_shared<SpriteAnimAssetData>();
  data->loop = j.value("loop", true);

  for (auto& fj : j["frames"]) {
    SpriteAnimAssetData::Frame frame;
    std::string sprite_ref;
    if (fj.is_object()) {
      sprite_ref = fj.value("sprite", "");
      frame.duration = fj.value("duration", 0.1f);
    } else if (fj.is_string()) {
      sprite_ref = fj.get<std::string>();
      frame.duration = 0.1f;
    } else {
      continue;
    }

    if (sprite_ref.empty()) {
      continue;
    }

    frame.sprite_handle = AssetHandle::FromString(sprite_ref);
    if (!frame.sprite_handle.IsValid()) {
      continue;
    }

    // Ensure the .wsprite is loaded
    auto sprite_data =
        Engine::asset_manager().Get<SpriteAssetData>(frame.sprite_handle);
    if (!sprite_data) {
      Engine::asset_manager().LoadSync(frame.sprite_handle);
    }

    Engine::asset_manager().AddDependency(handle, frame.sprite_handle);
    data->frames.push_back(std::move(frame));
  }

  if (data->frames.empty()) {
    LOG_ERROR("SpriteAnim: no valid frames");
    return false;
  }

  Engine::asset_manager().Store(handle, data);
  Engine::asset_manager().SetLoadState(handle, AssetLoadState::Unloaded,
                                       AssetLoadState::Loaded);
  LOG_INFO("Loaded sprite anim: {} frames, loop={}", data->frames.size(),
           data->loop);
  return true;
}

static void ParseTransitionConditions(
    const nlohmann::json& conds_json,
    std::vector<TransitionCondition>& out_conditions) {
  for (auto& condj : conds_json) {
    TransitionCondition cond;
    cond.param_name = condj.value("param", "");

    std::string type_str = condj.value("type", "Bool");
    if (type_str == "Bool") {
      cond.param_type = AnimParamType::Bool;
    } else if (type_str == "Int") {
      cond.param_type = AnimParamType::Int;
    } else if (type_str == "Float") {
      cond.param_type = AnimParamType::Float;
    } else if (type_str == "Trigger") {
      cond.param_type = AnimParamType::Trigger;
    }

    std::string op_str = condj.value("op", "Equals");
    if (op_str == "Equals") {
      cond.op = ConditionOp::Equals;
    } else if (op_str == "NotEquals") {
      cond.op = ConditionOp::NotEquals;
    } else if (op_str == "Greater") {
      cond.op = ConditionOp::Greater;
    } else if (op_str == "Less") {
      cond.op = ConditionOp::Less;
    }

    if (cond.param_type == AnimParamType::Bool ||
        cond.param_type == AnimParamType::Trigger) {
      cond.value.b = condj.value("value", true);
    } else if (cond.param_type == AnimParamType::Int) {
      cond.value.i = condj.value("value", 0);
    } else if (cond.param_type == AnimParamType::Float) {
      cond.value.f = condj.value("value", 0.0f);
    }

    if (!cond.param_name.empty()) {
      out_conditions.push_back(std::move(cond));
    }
  }
}

bool LoadSpriteControllerAsset(const AssetHandle& handle) {
  auto j = LoadJsonAsset(handle);
  if (j.is_null()) {
    return false;
  }

  auto data = std::make_shared<SpriteControllerAssetData>();
  data->default_state = j.value("default_state", "");

  // Load states
  if (j.contains("states") && j["states"].is_array()) {
    for (auto& sj : j["states"]) {
      SpriteControllerAssetData::State state;
      state.name = sj.value("name", "");
      std::string anim_ref = sj.value("animation", "");
      if (!anim_ref.empty()) {
        state.animation_handle = AssetHandle::FromString(anim_ref);
      }
      state.speed = sj.value("speed", 1.0f);

      if (state.name.empty()) {
        continue;
      }

      // Ensure the .wspriteanim is loaded
      if (state.animation_handle.IsValid()) {
        auto anim_data = Engine::asset_manager().Get<SpriteAnimAssetData>(
            state.animation_handle);
        if (!anim_data) {
          Engine::asset_manager().LoadSync(state.animation_handle);
        }
        Engine::asset_manager().AddDependency(handle, state.animation_handle);
      }

      data->states.push_back(std::move(state));
    }
  }

  // Load transitions
  if (j.contains("transitions") && j["transitions"].is_array()) {
    for (auto& tj : j["transitions"]) {
      AnimationTransition trans;
      trans.from_state = tj.value("from", "");
      trans.to_state = tj.value("to", "");
      trans.blend_duration = tj.value("blend", 0.0f);

      if (tj.contains("conditions") && tj["conditions"].is_array()) {
        ParseTransitionConditions(tj["conditions"], trans.conditions);
      }

      if (!trans.to_state.empty()) {
        data->transitions.push_back(std::move(trans));
      }
    }
  }

  Engine::asset_manager().Store(handle, data);
  Engine::asset_manager().SetLoadState(handle, AssetLoadState::Unloaded,
                                       AssetLoadState::Loaded);
  LOG_INFO("Loaded sprite controller: {} states, {} transitions",
           data->states.size(), data->transitions.size());
  return true;
}

}  // namespace Wiesel
