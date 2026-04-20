
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_transient_resource_pool.h"

#include "rendering/w_renderer.h"

namespace wiesel {

TransientResourcePool::TransientResourcePool(Renderer& renderer)
    : renderer_(renderer) {}

TransientResourcePool::~TransientResourcePool() = default;

std::shared_ptr<AttachmentTexture> TransientResourcePool::Acquire(
    const AttachmentTextureProps& props) {
  uint64_t hash = props.Hash();
  auto& bucket = buckets_[hash];
  for (auto& entry : bucket) {
    if (!entry.in_use && entry.props == props) {
      entry.in_use = true;
      entry.last_used_frame = frame_;
      return entry.texture;
    }
  }

  Entry entry;
  entry.props = props;
  entry.texture = renderer_.CreateAttachmentTexture(props);
  entry.last_used_frame = frame_;
  entry.in_use = true;
  auto texture = entry.texture;
  bucket.push_back(std::move(entry));
  return texture;
}

void TransientResourcePool::Release(
    const std::shared_ptr<AttachmentTexture>& texture) {
  if (!texture) {
    return;
  }
  for (auto& [hash, bucket] : buckets_) {
    for (auto& entry : bucket) {
      if (entry.texture == texture) {
        entry.in_use = false;
        return;
      }
    }
  }
}

void TransientResourcePool::BeginFrame() {
  frame_++;

  // Evict idle entries across all buckets. We check both the idle age and
  // the in_use flag so a resource that's held across a frame boundary
  // (shouldn't happen, but defensive) doesn't get yanked out.
  for (auto it = buckets_.begin(); it != buckets_.end();) {
    auto& bucket = it->second;
    for (auto eit = bucket.begin(); eit != bucket.end();) {
      if (!eit->in_use && frame_ - eit->last_used_frame > kMaxIdleFrames) {
        eit = bucket.erase(eit);
      } else {
        ++eit;
      }
    }
    if (bucket.empty()) {
      it = buckets_.erase(it);
    } else {
      ++it;
    }
  }
}

void TransientResourcePool::Clear() {
  buckets_.clear();
}

}  // namespace wiesel
