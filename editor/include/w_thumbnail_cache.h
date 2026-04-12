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

#include <imgui.h>
#include <unordered_map>
#include <unordered_set>
#include "asset/w_asset_handle.h"

namespace Wiesel {

struct AssetMetadata;

struct ThumbnailEntry {
  VkDescriptorSet texture_id = nullptr;
  bool attempted = false;
  uint32_t width = 0;
  uint32_t height = 0;
  ImVec2 uv0 = {0.0f, 0.0f};
  ImVec2 uv1 = {1.0f, 1.0f};

  ImVec2 FitSize(float max_size) const {
    float w = static_cast<float>(width) * (uv1.x - uv0.x);
    float h = static_cast<float>(height) * (uv1.y - uv0.y);
    if (w <= 0 || h <= 0) {
      return ImVec2(max_size, max_size);
    }
    float aspect = w / h;
    if (aspect >= 1.0f) {
      return ImVec2(max_size, max_size / aspect);
    }
    return ImVec2(max_size * aspect, max_size);
  }

  // Visible dimensions (pixel size of the sub-region)
  uint32_t VisibleWidth() const {
    return static_cast<uint32_t>(width * (uv1.x - uv0.x));
  }

  uint32_t VisibleHeight() const {
    return static_cast<uint32_t>(height * (uv1.y - uv0.y));
  }
};

class ThumbnailCache {
 public:
  // Get or create a thumbnail for an asset. Returns a default entry if
  // the thumbnail can't be created yet.
  ThumbnailEntry GetOrCreate(AssetHandle handle, const AssetMetadata& meta);

  // Remove a specific entry (e.g. on asset unload).
  void Remove(AssetHandle handle);

  // Remove entries for assets that no longer exist.
  void RemoveStale();

  // Clear all entries.
  void Clear();

  // Global accessor - set by the editor on startup.
  static ThumbnailCache* Get() { return instance_; }

  static void Set(ThumbnailCache* cache) { instance_ = cache; }

 private:
  std::unordered_map<AssetHandle, ThumbnailEntry> cache_;
  std::unordered_set<AssetHandle> pending_;  // async loads in flight
  static ThumbnailCache* instance_;
};

}  // namespace Wiesel