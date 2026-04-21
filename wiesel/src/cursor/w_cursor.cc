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
#include <algorithm>
#include <cstring>

#include "asset/w_asset_manager.h"
#include "rendering/w_sprite_asset.h"
#include "util/w_vfs.h"
#include "w_engine.h"

namespace wiesel {

static void LoadFramePixels(CursorFrame& frame) {
  if (!frame.texture.IsValid()) {
    return;
  }

  AssetManager& mgr = Engine::asset_manager();
  const AssetMetadata* meta = mgr.GetMetadata(frame.texture);
  if (!meta) {
    return;
  }

  std::string image_vfs_path;
  // {x, y, w, h} in pixels; w/h == 0 means "use the whole image".
  glm::ivec4 crop = {0, 0, 0, 0};

  if (meta->type == AssetType::Sprite) {
    // Cursor frames load lazily: GetOrStartLoad kicks off an async load and
    // returns null until ready. CursorManager::Update retries each tick and
    // calls ApplyCurrentFrame once the pixels are populated.
    auto sprite = mgr.GetOrStartLoad<SpriteAssetData>(frame.texture);
    if (!sprite || !sprite->texture_handle.IsValid()) {
      return;
    }
    const AssetMetadata* tex_meta = mgr.GetMetadata(sprite->texture_handle);
    if (!tex_meta) {
      return;
    }
    image_vfs_path = tex_meta->virtual_source_path;
    crop = {static_cast<int>(sprite->rect.x),
            static_cast<int>(sprite->rect.y),
            static_cast<int>(sprite->rect.z),
            static_cast<int>(sprite->rect.w)};
  } else if (meta->type == AssetType::Texture) {
    image_vfs_path = meta->virtual_source_path;
  } else {
    LOG_WARN("Cursor frame asset '{}' is type {}; expected Texture or Sprite",
             meta->name, AssetTypeToString(meta->type));
    return;
  }

  VfsFile file = Engine::vfs()->Open(image_vfs_path);
  if (!file) {
    return;
  }

  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc* px =
      stbi_load_from_memory(file.Data(), static_cast<int>(file.Size()), &w, &h,
                            &channels, STBI_rgb_alpha);
  if (!px) {
    return;
  }

  if (crop.z > 0 && crop.w > 0) {
    int cx = std::clamp(crop.x, 0, w);
    int cy = std::clamp(crop.y, 0, h);
    int cw = std::clamp(crop.z, 0, w - cx);
    int ch = std::clamp(crop.w, 0, h - cy);
    auto pixels = std::make_shared<std::vector<uint8_t>>(cw * ch * 4);
    for (int row = 0; row < ch; row++) {
      std::memcpy(pixels->data() + row * cw * 4,
                  px + ((cy + row) * w + cx) * 4, cw * 4);
    }
    frame.pixels = std::move(pixels);
    frame.pixel_width = cw;
    frame.pixel_height = ch;
  } else {
    frame.pixels = std::make_shared<std::vector<uint8_t>>(px, px + w * h * 4);
    frame.pixel_width = w;
    frame.pixel_height = h;
  }
  stbi_image_free(px);
}

void CursorManager::SetCursorSet(AssetHandle handle) {
  if (cursor_set_handle_ == handle && cursor_set_) {
    return;
  }

  Clear();

  if (!handle.IsValid()) {
    return;
  }

  cursor_set_handle_ = handle;
  current_state_ = "default";
  current_frame_ = 0;
  frame_timer_ = 0.0f;

  // Kick off async load. If the asset is already loaded we take it now;
  // otherwise Update polls each tick and applies the cursor as soon as it
  // lands. The OS default cursor stays in the meantime.
  cursor_set_ =
      Engine::asset_manager().GetOrStartLoad<CursorSetData>(handle);
  if (cursor_set_) {
    ApplyCurrentFrame();
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

  if (!cursor_set_->states.contains(state)) {
    LOG_WARN("Cursor state '{}' not found in cursor set", state);
    return;
  }

  ApplyCurrentFrame();
}

void CursorManager::Update(float delta_time) {
  // Cursor set asset may still be loading from when SetCursorSet was called.
  // Poll until it lands, then apply.
  if (!cursor_set_ && cursor_set_handle_.IsValid()) {
    cursor_set_ = Engine::asset_manager().GetOrStartLoad<CursorSetData>(
        cursor_set_handle_);
    if (cursor_set_) {
      ApplyCurrentFrame();
    }
  }
  if (!cursor_set_) {
    return;
  }

  auto it = cursor_set_->states.find(current_state_);
  if (it == cursor_set_->states.end()) {
    return;
  }

  CursorStateEntry& entry = it->second;
  if (entry.frames.empty()) {
    return;
  }

  // Frame pixels load asynchronously - if the current frame wasn't ready when
  // we last tried to apply, poll and re-apply once it shows up. The OS keeps
  // its previous cursor in the meantime.
  CursorFrame& current = entry.frames[current_frame_];
  if (!current.pixels) {
    LoadFramePixels(current);
    if (current.pixels) {
      ApplyCurrentFrame();
    }
  }

  if (entry.frames.size() <= 1) {
    return;  // Not animated
  }

  frame_timer_ += delta_time;
  if (frame_timer_ >= entry.frame_duration) {
    frame_timer_ -= entry.frame_duration;
    current_frame_ =
        (current_frame_ + 1) % static_cast<int>(entry.frames.size());

    ApplyCurrentFrame();
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

void CursorManager::RefreshCursorSet(AssetHandle handle) {
  if (cursor_set_handle_ != handle || !handle.IsValid() || !cursor_set_) {
    return;
  }
  // Drop cached pixel buffers in place; Update / ApplyCurrentFrame will
  // lazy-reload them for whatever frame textures the asset now points at.
  for (auto& [name, entry] : cursor_set_->states) {
    for (auto& frame : entry.frames) {
      frame.pixels.reset();
      frame.pixel_width = 0;
      frame.pixel_height = 0;
    }
  }
  ApplyCurrentFrame();
}

void CursorManager::SetActive(bool active) {
  if (active_ == active) {
    return;
  }
  active_ = active;
  if (active_) {
    ApplyCurrentFrame();
  } else {
    Engine::window()->ResetCustomCursor();
  }
}

void CursorManager::ApplyCurrentFrame() {
  if (!active_ || !cursor_set_ ||
      cursor_set_->mode != CursorSetMode::Auto) {
    return;
  }
  auto it = cursor_set_->states.find(current_state_);
  if (it == cursor_set_->states.end() || it->second.frames.empty()) {
    return;
  }
  int frame_idx = std::clamp(current_frame_, 0,
                             static_cast<int>(it->second.frames.size()) - 1);
  CursorFrame& frame = it->second.frames[frame_idx];
  if (!frame.pixels) {
    // Best-effort kick off; if not ready, leave the previous cursor in place
    // and let Update poll until it lands.
    LoadFramePixels(frame);
    if (!frame.pixels) {
      return;
    }
  }
  ApplyHardwareCursor(frame, it->second.hotspot, it->second.scale);
}

void CursorManager::ApplyHardwareCursor(const CursorFrame& frame,
                                        const glm::ivec2& hotspot, int scale) {
  if (!frame.pixels || frame.pixels->empty()) {
    Engine::window()->ResetCustomCursor();
    return;
  }

  scale = std::clamp(scale, 1, 8);

  if (scale == 1) {
    Engine::window()->SetCustomCursor(frame.pixels->data(), frame.pixel_width,
                                      frame.pixel_height, hotspot.x,
                                      hotspot.y);
    return;
  }

  // Nearest-neighbor upscale. Cursors are tiny and scale infrequently, so the
  // extra allocation per apply is not worth caching.
  int src_w = frame.pixel_width;
  int src_h = frame.pixel_height;
  int dst_w = src_w * scale;
  int dst_h = src_h * scale;
  std::vector<uint8_t> scaled(static_cast<size_t>(dst_w) * dst_h * 4);
  const uint8_t* src = frame.pixels->data();
  for (int y = 0; y < dst_h; y++) {
    const uint8_t* src_row = src + (y / scale) * src_w * 4;
    uint8_t* dst_row = scaled.data() + static_cast<size_t>(y) * dst_w * 4;
    for (int x = 0; x < dst_w; x++) {
      const uint8_t* s = src_row + (x / scale) * 4;
      uint8_t* d = dst_row + x * 4;
      d[0] = s[0];
      d[1] = s[1];
      d[2] = s[2];
      d[3] = s[3];
    }
  }
  Engine::window()->SetCustomCursor(scaled.data(), dst_w, dst_h,
                                    hotspot.x * scale, hotspot.y * scale);
}

}  // namespace wiesel
