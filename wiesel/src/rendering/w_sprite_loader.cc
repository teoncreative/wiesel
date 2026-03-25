
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
#include "util/w_logger.h"
#include "util/w_vfs.h"
#include "w_engine.h"

namespace Wiesel {

static std::string ResolveAssetPath(const std::string& ref) {
  // If it looks like a UUID, resolve via asset manager
  if (ref.size() > 30 && ref.find('-') != std::string::npos) {
    AssetHandle h = AssetHandle::FromString(ref);
    if (h.IsValid()) {
      const auto* meta = Engine::asset_manager().GetMetadata(h);
      if (meta) {
        return meta->virtual_source_path;
      }
    }
  }
  // Otherwise treat as VFS path
  return ref;
}

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

std::shared_ptr<SpriteAsset> LoadSpriteSheet(const AssetHandle& handle) {
  auto j = LoadJsonAsset(handle);
  if (j.is_null()) {
    return nullptr;
  }

  // Check if it's multi-image ("textures" array) or single-image ("texture" string)
  bool is_multi = j.contains("textures") && j["textures"].is_array();

  std::shared_ptr<SpriteAsset> asset;

  if (is_multi) {
    // Multi-image mode: stitch individual frames
    std::vector<std::string> frame_paths;
    for (auto& t : j["textures"]) {
      std::string path = ResolveAssetPath(t.get<std::string>());
      if (!path.empty()) {
        frame_paths.push_back(path);
      }
    }
    if (frame_paths.empty()) {
      LOG_ERROR("SpriteSheet: no valid textures in array");
      return nullptr;
    }

    float frame_duration = j.value("frame_duration", 0.1f);
    SpriteBuilder builder(frame_paths);
    // Frame duration can be overridden; frames are auto-added by Build()
    asset = builder.Build();

    // Override durations if specified
    if (asset) {
      for (auto& frame : asset->GetFrames()) {
        frame.duration = frame_duration;
      }
    }
  } else {
    // Single atlas mode
    std::string texture_ref = j.value("texture", "");
    std::string texture_path = ResolveAssetPath(texture_ref);
    if (texture_path.empty()) {
      LOG_ERROR("SpriteSheet: missing texture");
      return nullptr;
    }

    auto cell_size_arr = j.value("cell_size", nlohmann::json::array({64, 64}));
    glm::ivec2 cell_size(cell_size_arr[0].get<int>(),
                         cell_size_arr[1].get<int>());
    int frame_count = j.value("frame_count", 0);

    VfsFile tex_file = Engine::vfs()->Open(texture_path);
    if (!tex_file) {
      LOG_ERROR("SpriteSheet: texture not found: {}", texture_path);
      return nullptr;
    }
    int tex_w, tex_h, tex_ch;
    stbi_uc* pixels = stbi_load_from_memory(
        tex_file.Data(), static_cast<int>(tex_file.Size()), &tex_w, &tex_h,
        &tex_ch, STBI_rgb_alpha);
    if (!pixels) {
      LOG_ERROR("SpriteSheet: failed to load texture: {}", texture_path);
      return nullptr;
    }
    stbi_image_free(pixels);

    glm::vec2 atlas_size(tex_w, tex_h);
    int cols = tex_w / cell_size.x;
    int rows = tex_h / cell_size.y;
    int total_frames = frame_count > 0 ? frame_count : (cols * rows);

    SpriteBuilder builder(texture_path, atlas_size);
    builder.SetFixedSize(glm::vec2(cell_size));
    builder.AddGridFrames(cell_size, 0, 0, total_frames, 0.1f);
    asset = builder.Build();
  }

  if (asset) {
    Engine::asset_manager().Store<SpriteAsset>(handle, asset);
    Engine::asset_manager().SetLoadState(handle, AssetLoadState::Unloaded,
                                         AssetLoadState::Loaded);
    LOG_INFO("Loaded sprite sheet: {} frames", asset->GetFrames().size());
  }
  return asset;
}

bool LoadSpriteAnim(const AssetHandle& handle, SpriteComponent& out) {
  auto j = LoadJsonAsset(handle);
  if (j.is_null()) {
    return false;
  }

  // Load the referenced sprite sheet
  std::string sheet_ref = j.value("sprite_sheet", "");
  AssetHandle sheet_handle;
  if (!sheet_ref.empty()) {
    sheet_handle = AssetHandle::FromString(sheet_ref);
  }

  if (!sheet_handle.IsValid()) {
    LOG_ERROR("SpriteAnim: missing sprite_sheet reference");
    return false;
  }

  // Get or load the sprite sheet
  auto sprite_asset = Engine::asset_manager().Get<SpriteAsset>(sheet_handle);
  if (!sprite_asset) {
    sprite_asset = LoadSpriteSheet(sheet_handle);
  }
  if (!sprite_asset) {
    LOG_ERROR("SpriteAnim: failed to load sprite sheet");
    return false;
  }

  out.asset_ = sprite_asset;
  out.clips.clear();

  // Load clips
  if (j.contains("clips") && j["clips"].is_array()) {
    for (auto& cj : j["clips"]) {
      SpriteClip clip;
      clip.name = cj.value("name", "");
      clip.start_frame = cj.value("start", 0);
      clip.frame_count = cj.value("count", 1);
      clip.frame_duration = cj.value("duration", 0.1f);
      clip.loop = cj.value("loop", true);
      if (!clip.name.empty()) {
        out.clips.push_back(std::move(clip));
      }
    }
  }

  // Load controller (optional)
  if (j.contains("controller") && j["controller"].is_object()) {
    auto& cj = j["controller"];
    auto& ctrl = out.state_machine.controller;

    ctrl.default_state = cj.value("default_state", "");

    if (cj.contains("states") && cj["states"].is_array()) {
      for (auto& sj : cj["states"]) {
        AnimationState state;
        state.name = sj.value("name", "");
        state.clip_name = sj.value("clip_name", "");
        state.speed = sj.value("speed", 1.0f);
        state.looping = sj.value("looping", true);
        if (!state.name.empty()) {
          ctrl.states.push_back(std::move(state));
        }
      }
    }

    if (cj.contains("transitions") && cj["transitions"].is_array()) {
      for (auto& tj : cj["transitions"]) {
        AnimationTransition trans;
        trans.from_state = tj.value("from", "");
        trans.to_state = tj.value("to", "");
        trans.blend_duration = tj.value("blend", 0.0f);

        if (tj.contains("conditions") && tj["conditions"].is_array()) {
          for (auto& condj : tj["conditions"]) {
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
              trans.conditions.push_back(std::move(cond));
            }
          }
        }

        if (!trans.to_state.empty()) {
          ctrl.transitions.push_back(std::move(trans));
        }
      }
    }
  }

  // Auto-play default state or first clip
  if (!out.state_machine.controller.IsEmpty()) {
    out.state_machine.EnsureDefaultState();
    const auto* state = out.state_machine.GetCurrentState();
    if (state) {
      const SpriteClip* clip = out.FindClip(state->clip_name);
      if (clip) {
        out.current_frame_ = clip->start_frame;
      }
    }
  } else if (!out.clips.empty()) {
    out.Play(out.clips[0].name);
  }

  LOG_INFO("Loaded sprite anim: {} clips, {} states", out.clips.size(),
           out.state_machine.controller.states.size());
  return true;
}

}  // namespace Wiesel