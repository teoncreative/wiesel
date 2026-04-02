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

#include "asset/w_asset_handle.h"
#include "w_pch.h"

namespace Wiesel {

enum class CursorSetMode : uint8_t {
  Auto = 0,           // Use OS hardware cursor (SDL)
  ForceSoftware = 1,  // Engine-rendered cursor quad
};

struct CursorFrame {
  AssetHandle texture;

  // Cached RGBA pixel data (loaded once when cursor set is activated)
  std::shared_ptr<std::vector<uint8_t>> pixels;
  int pixel_width = 0;
  int pixel_height = 0;
};

struct CursorStateEntry {
  glm::ivec2 hotspot{0, 0};
  glm::ivec2 size{32, 32};
  float frame_duration = 0.1f;  // Seconds per frame (for animated cursors)
  std::vector<CursorFrame> frames;
};

struct CursorSetData {
  CursorSetMode mode = CursorSetMode::Auto;
  std::unordered_map<std::string, CursorStateEntry> states;
};

class CursorManager {
 public:
  // Set the active cursor set from an asset handle.
  void SetCursorSet(AssetHandle handle);

  // Switch to a named cursor state (e.g. "default", "pointer", "crosshair").
  void SetCursorState(const std::string& state);

  // Tick animation (call once per frame with delta time).
  void Update(float delta_time);

  // Get the current cursor state name.
  const std::string& GetCursorState() const { return current_state_; }

  // Get the active cursor set handle.
  AssetHandle GetCursorSetHandle() const { return cursor_set_handle_; }

  // Clear the cursor set and reset to OS default.
  void Clear();

 private:
  void ApplyHardwareCursor(const CursorFrame& frame, const glm::ivec2& hotspot);

  AssetHandle cursor_set_handle_;
  std::shared_ptr<CursorSetData> cursor_set_;
  std::string current_state_ = "default";
  int current_frame_ = 0;
  float frame_timer_ = 0.0f;
};

}  // namespace Wiesel
