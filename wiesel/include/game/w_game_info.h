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

#pragma once

#include "asset/w_asset_handle.h"
#include "w_pch.h"

namespace Wiesel {

struct RenderOptionsSerialized {
  glm::vec3 ambient_color = {1.0f, 1.0f, 1.0f};
  float ambient_intensity = 0.3f;
  bool ssao_enabled = true;
  bool ibl_enabled = true;
  bool bloom_enabled = true;
  float bloom_threshold = 0.9f;
  float bloom_intensity = 0.0f;
  float bloom_scatter = 0.7f;
  glm::vec3 bloom_tint = glm::vec3(1.0f);
  float bloom_clamp = 65472.0f;
  bool bloom_high_quality = false;
  bool motion_blur_enabled = false;
  float motion_blur_strength = 1.0f;
  int motion_blur_samples = 8;
  bool shadows_enabled = true;
  bool vsync = false;
  int aa_mode = 0;    // 0=None, 1=FXAA, 2=TAA
  int msaa_mode = 0;  // SamplingMode enum value
  int shadow_resolution = 4096;
  int anisotropic_filtering = 16;
  int texture_quality = 0;  // mip bias: 0=Full, 1=Half, 2=Quarter, 3=Eighth
};

// Input action: a named action mapped to keyboard keys and/or gamepad buttons
struct InputAction {
  std::string name;              // e.g. "Jump", "Fire"
  std::vector<int32_t> keys;     // KeyCode values
  std::vector<int32_t> buttons;  // GamepadButton values
};

// Input axis: mapped to keys (digital) and/or a gamepad stick/trigger (analog)
struct InputAxisMapping {
  std::string name;                    // e.g. "Horizontal", "Vertical"
  std::vector<int32_t> positive_keys;  // Keys that push toward +1
  std::vector<int32_t> negative_keys;  // Keys that push toward -1
  int32_t gamepad_axis = -1;           // GamepadAxis (-1 = none)
  bool invert_axis = false;            // Flip gamepad axis direction
  float dead_zone = 0.15f;
  bool smooth = false;       // Enable smoothing for digital axes
  float gravity = 3.0f;      // How fast digital axis returns to 0 (when smooth)
  float sensitivity = 3.0f;  // How fast digital axis reaches 1/-1 (when smooth)
};

// A named set of input bindings (e.g. "keyboard", "gamepad", "keyboard_p2")
struct InputContext {
  std::string name;
  std::vector<InputAction> actions;
  std::vector<InputAxisMapping> axes;
};

struct InputSettings {
  std::map<std::string, InputContext> contexts;  // name -> context
  float mouse_sensitivity_x = 80.0f;
  float mouse_sensitivity_y = 80.0f;
};

// Runtime game configuration - loaded from gameinfo.wgame.
// Contains everything a shipped game needs to start.
struct GameInfo {
  std::string name = "Untitled Game";
  std::string version = "1.0.0";
  AssetHandle icon;
  AssetHandle start_scene;
  RenderOptionsSerialized render_options;
  InputSettings input;
  std::vector<std::string> search_paths;  // ordered pak search directories

  static std::unique_ptr<GameInfo> Load(const std::filesystem::path& path);
  bool Save(const std::filesystem::path& path) const;
};

}  // namespace Wiesel
