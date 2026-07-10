
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

#include "rendering/w_texture.h"
#include "w_pch.h"

namespace wiesel {

class Renderer;

// Frame-graph style transient image pool. RenderGraph asks for textures by
// descriptor every frame; non-overlapping transients with matching descriptors
// share the same backing image across compiles, and entries idle for several
// frames are evicted. Nothing persists on the graph side - the pool is the
// only long-lived state for transients.
class TransientResourcePool {
 public:
  explicit TransientResourcePool(Renderer& renderer);
  ~TransientResourcePool();

  TransientResourcePool(const TransientResourcePool&) = delete;
  TransientResourcePool& operator=(const TransientResourcePool&) = delete;

  // Return a matching free entry, creating a new one if none exists. The
  // returned texture is owned by the pool; call Release() when the graph is
  // done with it for the frame.
  std::shared_ptr<AttachmentTexture> Acquire(
      const AttachmentTextureProps& props);

  // Return an acquired texture to the pool's free list. Safe to call with an
  // unknown pointer (no-op). Pointer equality on the stored shared_ptr.
  void Release(const std::shared_ptr<AttachmentTexture>& texture);

  // Bumps the frame counter and drops entries unused for > kMaxIdleFrames
  // frames. Call once per frame before the graph starts compiling.
  void BeginFrame();

  // Drop every entry, in-use or not. Used on resource-affecting settings
  // changes (MSAA toggle, resize, etc.).
  void Clear();

  // Number of VkImage-backed entries the pool is currently holding. Useful
  // for debug panels - spikes indicate either misaliasing or rapid
  // descriptor churn.
  size_t GetEntryCount() const {
    size_t count = 0;
    for (const auto& [hash, bucket] : buckets_) {
      count += bucket.size();
    }
    return count;
  }

 private:
  struct Entry {
    AttachmentTextureProps props;
    std::shared_ptr<AttachmentTexture> texture;
    uint64_t last_used_frame = 0;
    bool in_use = false;
  };

  static constexpr uint32_t kMaxIdleFrames = 8;

  Renderer& renderer_;
  // Bucketed by props.Hash() so Acquire doesn't linear-scan the whole pool.
  // Collisions still fall back to operator== within the bucket for safety.
  std::unordered_map<uint64_t, std::vector<Entry>> buckets_;
  uint64_t frame_ = 0;
};

}  // namespace wiesel
