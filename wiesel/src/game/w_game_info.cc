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
// Created by Metehan Gezer on 24.03.2026.
//

#include "game/w_game_info.h"

#include <nlohmann/json.hpp>

#include "util/w_gamepadcodes.h"
#include "util/w_keycodes.h"
#include "util/w_logger.h"

namespace Wiesel {

// --- Input serialization helpers ---

static nlohmann::json SerializeAction(const InputAction& action) {
  nlohmann::json aj;
  aj["name"] = action.name;

  nlohmann::json keys = nlohmann::json::array();
  for (auto k : action.keys) {
    keys.push_back(KeyCodeToString(k));
  }
  aj["keys"] = keys;

  if (!action.buttons.empty()) {
    nlohmann::json btns = nlohmann::json::array();
    for (auto b : action.buttons) {
      btns.push_back(GamepadButtonToString(b));
    }
    aj["buttons"] = btns;
  }
  return aj;
}

static nlohmann::json SerializeAxis(const InputAxisMapping& axis) {
  nlohmann::json axj;
  axj["name"] = axis.name;

  nlohmann::json pos = nlohmann::json::array();
  for (auto k : axis.positive_keys) {
    pos.push_back(KeyCodeToString(k));
  }
  axj["positive_keys"] = pos;

  nlohmann::json neg = nlohmann::json::array();
  for (auto k : axis.negative_keys) {
    neg.push_back(KeyCodeToString(k));
  }
  axj["negative_keys"] = neg;

  if (axis.gamepad_axis >= 0) {
    axj["stick"] = GamepadAxisToString(axis.gamepad_axis);
    if (axis.invert_axis) {
      axj["invert"] = true;
    }
    if (axis.dead_zone != 0.15f) {
      axj["dead_zone"] = axis.dead_zone;
    }
  }

  if (axis.smooth) {
    axj["smooth"] = true;
  }
  axj["gravity"] = axis.gravity;
  axj["sensitivity"] = axis.sensitivity;
  return axj;
}

static InputAction DeserializeAction(const nlohmann::json& aj) {
  InputAction action;
  action.name = aj.value("name", "");

  // Keys: string or legacy integer
  if (aj.contains("keys") && aj["keys"].is_array()) {
    for (auto& k : aj["keys"]) {
      int32_t code = k.is_string() ? StringToKeyCode(k.get<std::string>())
                                   : k.get<int32_t>();
      if (code != KeyUnknown) {
        action.keys.push_back(code);
      }
    }
  }

  // Gamepad buttons
  if (aj.contains("buttons") && aj["buttons"].is_array()) {
    for (auto& b : aj["buttons"]) {
      int32_t btn = b.is_string() ? StringToGamepadButton(b.get<std::string>())
                                  : b.get<int32_t>();
      if (btn >= 0) {
        action.buttons.push_back(btn);
      }
    }
  }
  return action;
}

static InputAxisMapping DeserializeAxis(const nlohmann::json& axj) {
  InputAxisMapping axis;
  axis.name = axj.value("name", "");
  axis.smooth = axj.value("smooth", false);
  axis.gravity = axj.value("gravity", 3.0f);
  axis.sensitivity = axj.value("sensitivity", 3.0f);

  auto parse_key = [](const nlohmann::json& k) -> int32_t {
    return k.is_string() ? StringToKeyCode(k.get<std::string>())
                         : k.get<int32_t>();
  };

  if (axj.contains("positive_keys") && axj["positive_keys"].is_array()) {
    for (auto& k : axj["positive_keys"]) {
      int32_t code = parse_key(k);
      if (code != KeyUnknown) {
        axis.positive_keys.push_back(code);
      }
    }
  }
  if (axj.contains("negative_keys") && axj["negative_keys"].is_array()) {
    for (auto& k : axj["negative_keys"]) {
      int32_t code = parse_key(k);
      if (code != KeyUnknown) {
        axis.negative_keys.push_back(code);
      }
    }
  }

  // Gamepad axis
  if (axj.contains("stick") && axj["stick"].is_string()) {
    axis.gamepad_axis = StringToGamepadAxis(axj["stick"].get<std::string>());
    axis.invert_axis = axj.value("invert", false);
    axis.dead_zone = axj.value("dead_zone", 0.15f);
  }

  return axis;
}

// --- Render options serialization ---

static void SerializeRenderOptions(nlohmann::json& j,
                                   const RenderOptionsSerialized& opts) {
  j["render_options"] = {
      {"ambient_color",
       {opts.ambient_color.x, opts.ambient_color.y, opts.ambient_color.z}},
      {"ambient_intensity", opts.ambient_intensity},
      {"ssao_enabled", opts.ssao_enabled},
      {"ibl_enabled", opts.ibl_enabled},
      {"bloom_enabled", opts.bloom_enabled},
      {"bloom_threshold", opts.bloom_threshold},
      {"bloom_intensity", opts.bloom_intensity},
      {"motion_blur_enabled", opts.motion_blur_enabled},
      {"motion_blur_strength", opts.motion_blur_strength},
      {"motion_blur_samples", opts.motion_blur_samples},
      {"shadows_enabled", opts.shadows_enabled},
      {"vsync", opts.vsync},
      {"aa_mode", opts.aa_mode},
      {"msaa_mode", opts.msaa_mode},
      {"shadow_resolution", opts.shadow_resolution},
      {"anisotropic_filtering", opts.anisotropic_filtering},
      {"texture_quality", opts.texture_quality},
  };
}

static void DeserializeRenderOptions(const nlohmann::json& j,
                                     RenderOptionsSerialized& opts) {
  if (!j.contains("render_options")) {
    return;
  }
  auto& ro = j["render_options"];
  if (ro.contains("ambient_color") && ro["ambient_color"].is_array()) {
    opts.ambient_color = {ro["ambient_color"][0], ro["ambient_color"][1],
                          ro["ambient_color"][2]};
  }
  opts.ambient_intensity = ro.value("ambient_intensity", 0.3f);
  opts.ssao_enabled = ro.value("ssao_enabled", true);
  opts.ibl_enabled = ro.value("ibl_enabled", true);
  opts.bloom_enabled = ro.value("bloom_enabled", false);
  opts.bloom_threshold = ro.value("bloom_threshold", 0.7f);
  opts.bloom_intensity = ro.value("bloom_intensity", 0.6f);
  opts.motion_blur_enabled = ro.value("motion_blur_enabled", false);
  opts.motion_blur_strength = ro.value("motion_blur_strength", 1.0f);
  opts.motion_blur_samples = ro.value("motion_blur_samples", 8);
  opts.shadows_enabled = ro.value("shadows_enabled", true);
  opts.vsync = ro.value("vsync", false);
  opts.aa_mode = ro.value("aa_mode", 0);
  opts.msaa_mode = ro.value("msaa_mode", 0);
  opts.shadow_resolution = ro.value("shadow_resolution", 4096);
  opts.anisotropic_filtering = ro.value("anisotropic_filtering", 16);
  opts.texture_quality = ro.value("texture_quality", 0);
}

// --- Input settings serialization ---

static void SerializeInputSettings(nlohmann::json& j,
                                   const InputSettings& input) {
  nlohmann::json ij;
  ij["mouse_sensitivity_x"] = input.mouse_sensitivity_x;
  ij["mouse_sensitivity_y"] = input.mouse_sensitivity_y;

  nlohmann::json contexts_json = nlohmann::json::object();
  for (auto& [ctx_name, ctx] : input.contexts) {
    nlohmann::json cj;

    nlohmann::json actions_json = nlohmann::json::array();
    for (auto& action : ctx.actions) {
      actions_json.push_back(SerializeAction(action));
    }
    cj["actions"] = actions_json;

    nlohmann::json axes_json = nlohmann::json::array();
    for (auto& axis : ctx.axes) {
      axes_json.push_back(SerializeAxis(axis));
    }
    cj["axes"] = axes_json;

    contexts_json[ctx_name] = cj;
  }
  ij["contexts"] = contexts_json;
  j["input"] = ij;
}

static void DeserializeInputSettings(const nlohmann::json& j,
                                     InputSettings& input) {
  if (!j.contains("input")) {
    return;
  }
  auto& ij = j["input"];
  input.mouse_sensitivity_x = ij.value("mouse_sensitivity_x", 80.0f);
  input.mouse_sensitivity_y = ij.value("mouse_sensitivity_y", 80.0f);

  if (ij.contains("contexts") && ij["contexts"].is_object()) {
    for (auto& [ctx_name, ctx_json] : ij["contexts"].items()) {
      InputContext ctx;
      ctx.name = ctx_name;

      if (ctx_json.contains("actions") && ctx_json["actions"].is_array()) {
        for (auto& aj : ctx_json["actions"]) {
          auto action = DeserializeAction(aj);
          if (!action.name.empty()) {
            ctx.actions.push_back(std::move(action));
          }
        }
      }
      if (ctx_json.contains("axes") && ctx_json["axes"].is_array()) {
        for (auto& axj : ctx_json["axes"]) {
          auto axis = DeserializeAxis(axj);
          if (!axis.name.empty()) {
            ctx.axes.push_back(std::move(axis));
          }
        }
      }
      input.contexts[ctx_name] = std::move(ctx);
    }
  }

  // Legacy: flat actions/axes at input root -> migrate into "keyboard" context
  if (!ij.contains("contexts") &&
      (ij.contains("actions") || ij.contains("axes"))) {
    InputContext legacy;
    legacy.name = "keyboard";
    if (ij.contains("actions") && ij["actions"].is_array()) {
      for (auto& aj : ij["actions"]) {
        auto action = DeserializeAction(aj);
        if (!action.name.empty()) {
          legacy.actions.push_back(std::move(action));
        }
      }
    }
    if (ij.contains("axes") && ij["axes"].is_array()) {
      for (auto& axj : ij["axes"]) {
        auto axis = DeserializeAxis(axj);
        if (!axis.name.empty()) {
          legacy.axes.push_back(std::move(axis));
        }
      }
    }
    input.contexts["keyboard"] = std::move(legacy);
  }
}

// --- GameInfo ---

std::unique_ptr<GameInfo> GameInfo::Load(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open game info file: {}", path.string());
    return nullptr;
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const nlohmann::json::parse_error& e) {
    LOG_ERROR("Failed to parse game info file: {}", e.what());
    return nullptr;
  }

  auto info = std::make_unique<GameInfo>();
  info->name = j.value("name", "Untitled Game");
  info->version = j.value("version", "1.0.0");
  info->icon = AssetHandle::FromString(j.value("icon", ""));
  info->start_scene = AssetHandle::FromString(j.value("start_scene", ""));
  DeserializeRenderOptions(j, info->render_options);
  DeserializeInputSettings(j, info->input);
  return info;
}

bool GameInfo::Save(const std::filesystem::path& path) const {
  nlohmann::json j;
  j["name"] = name;
  j["version"] = version;
  j["icon"] = icon.IsValid() ? icon.ToString() : "";
  j["start_scene"] = start_scene.IsValid() ? start_scene.ToString() : "";
  SerializeRenderOptions(j, render_options);
  SerializeInputSettings(j, input);

  std::ofstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to save game info file: {}", path.string());
    return false;
  }
  file << j.dump(2);
  return true;
}

}  // namespace Wiesel
