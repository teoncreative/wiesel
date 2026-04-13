//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "cursor/w_cursor.h"

#include <stb_image.h>

#include "asset/w_asset_manager.h"
#include "util/w_vfs.h"
#include "w_engine.h"

namespace wiesel {

static void LoadFramePixels(CursorFrame& frame) {
  if (!frame.texture.IsValid()) {
    return;
  }
  const auto* meta = Engine::asset_manager().GetMetadata(frame.texture);
  if (!meta) {
    return;
  }
  VfsFile file = Engine::vfs()->Open(meta->virtual_source_path);
  if (!file) {
    return;
  }
  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc* px =
      stbi_load_from_memory(file.Data(), static_cast<int>(file.Size()), &w, &h,
                            &channels, STBI_rgb_alpha);
  if (px) {
    frame.pixels = std::make_shared<std::vector<uint8_t>>(px, px + w * h * 4);
    frame.pixel_width = w;
    frame.pixel_height = h;
    stbi_image_free(px);
  }
}

void CursorManager::SetCursorSet(AssetHandle handle) {
  if (cursor_set_handle_ == handle && cursor_set_) {
    return;
  }

  Clear();

  if (!handle.IsValid()) {
    return;
  }

  auto data = Engine::asset_manager().GetOrLoad<CursorSetData>(handle);
  if (!data) {
    LOG_ERROR("Failed to load cursor set: {}", handle.ToString());
    return;
  }

  // Pre-load pixel data for all frames in all states
  for (auto& [name, entry] : data->states) {
    for (auto& frame : entry.frames) {
      LoadFramePixels(frame);
    }
  }

  cursor_set_handle_ = handle;
  cursor_set_ = data;
  current_state_ = "default";
  current_frame_ = 0;
  frame_timer_ = 0.0f;

  // Apply the default state
  auto it = cursor_set_->states.find("default");
  if (it != cursor_set_->states.end() && !it->second.frames.empty()) {
    if (cursor_set_->mode == CursorSetMode::Auto) {
      ApplyHardwareCursor(it->second.frames[0], it->second.hotspot);
    }
  }
}

void CursorManager::SetCursorState(const std::string& state) {
  if (current_state_ == state) {
    return;
  }

  current_state_ = state;
  current_frame_ = 0;
  frame_timer_ = 0.0f;

  if (!cursor_set_) {
    return;
  }

  auto it = cursor_set_->states.find(state);
  if (it == cursor_set_->states.end()) {
    LOG_WARN("Cursor state '{}' not found in cursor set", state);
    return;
  }

  if (cursor_set_->mode == CursorSetMode::Auto && !it->second.frames.empty()) {
    ApplyHardwareCursor(it->second.frames[0], it->second.hotspot);
  }
}

void CursorManager::Update(float delta_time) {
  if (!cursor_set_) {
    return;
  }

  auto it = cursor_set_->states.find(current_state_);
  if (it == cursor_set_->states.end()) {
    return;
  }

  const auto& entry = it->second;
  if (entry.frames.size() <= 1) {
    return;  // Not animated
  }

  frame_timer_ += delta_time;
  if (frame_timer_ >= entry.frame_duration) {
    frame_timer_ -= entry.frame_duration;
    current_frame_ =
        (current_frame_ + 1) % static_cast<int>(entry.frames.size());

    if (cursor_set_->mode == CursorSetMode::Auto) {
      ApplyHardwareCursor(entry.frames[current_frame_], entry.hotspot);
    }
  }
}

void CursorManager::Clear() {
  cursor_set_.reset();
  cursor_set_handle_ = {};
  current_state_ = "default";
  current_frame_ = 0;
  frame_timer_ = 0.0f;
  Engine::window()->ResetCustomCursor();
}

void CursorManager::ApplyHardwareCursor(const CursorFrame& frame,
                                        const glm::ivec2& hotspot) {
  if (!frame.pixels || frame.pixels->empty()) {
    Engine::window()->ResetCustomCursor();
    return;
  }

  Engine::window()->SetCustomCursor(frame.pixels->data(), frame.pixel_width,
                                    frame.pixel_height, hotspot.x, hotspot.y);
}

}  // namespace wiesel
