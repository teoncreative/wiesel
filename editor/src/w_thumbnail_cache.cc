
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "../include/w_thumbnail_cache.h"

#include <backends/imgui_impl_vulkan.h>
#include "asset/w_asset_manager.h"
#include "rendering/w_sprite_asset.h"
#include "rendering/w_texture.h"
#include "w_engine.h"

namespace wiesel {

ThumbnailCache* ThumbnailCache::instance_ = nullptr;

// Try to build a thumbnail entry from an already-loaded texture.
// Returns true if successful, false if texture not ready yet.
static bool TryBuildTextureEntry(AssetHandle handle, ThumbnailEntry& entry) {
  auto texture = Engine::asset_manager().Get<Texture>(handle);
  if (!texture || !texture->is_allocated_ || !texture->image_view_ ||
      !texture->sampler_) {
    return false;
  }
  entry.texture_id = ImGui_ImplVulkan_AddTexture(
      texture->sampler_->handle(), texture->image_view_->handle_,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  entry.width = texture->width_;
  entry.height = texture->height_;
  entry.attempted = true;
  return true;
}

ThumbnailEntry ThumbnailCache::GetOrCreate(AssetHandle handle,
                                           const AssetMetadata& meta) {
  auto it = cache_.find(handle);
  if (it != cache_.end()) {
    return it->second;
  }

  ThumbnailEntry entry;
  AssetManager& mgr = Engine::asset_manager();

  if (meta.type == AssetType::Texture) {
    if (TryBuildTextureEntry(handle, entry)) {
      cache_[handle] = entry;
      pending_.erase(handle);
    } else if (!pending_.contains(handle)) {
      // Kick off async load, return empty for now
      pending_.insert(handle);
      mgr.LoadAsync(handle);
    }
  } else if (meta.type == AssetType::Sprite) {
    auto sprite_data = mgr.Get<SpriteAssetData>(handle);
    if (!sprite_data) {
      if (!pending_.contains(handle)) {
        pending_.insert(handle);
        mgr.LoadAsync(handle);
      }
      return entry;
    }

    if (!sprite_data->texture_handle.IsValid()) {
      return entry;
    }

    auto texture = mgr.Get<Texture>(sprite_data->texture_handle);
    if (!texture || !texture->is_allocated_ || !texture->image_view_ ||
        !texture->sampler_) {
      if (pending_.find(sprite_data->texture_handle) == pending_.end()) {
        pending_.insert(sprite_data->texture_handle);
        mgr.LoadAsync(sprite_data->texture_handle);
      }
      return entry;
    }

    float tw = static_cast<float>(texture->width_);
    float th = static_cast<float>(texture->height_);
    glm::vec4 uv = sprite_data->GetUVRect(tw, th);

    entry.texture_id = ImGui_ImplVulkan_AddTexture(
        texture->sampler_->handle(), texture->image_view_->handle_,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    entry.width = texture->width_;
    entry.height = texture->height_;
    entry.uv0 = ImVec2(uv.x, uv.y);
    entry.uv1 = ImVec2(uv.x + uv.z, uv.y + uv.w);
    entry.attempted = true;

    cache_[handle] = entry;
    pending_.erase(handle);
    pending_.erase(sprite_data->texture_handle);
  }

  return entry;
}

void ThumbnailCache::Remove(AssetHandle handle) {
  auto it = cache_.find(handle);
  if (it != cache_.end()) {
    if (it->second.texture_id) {
      ImGui_ImplVulkan_RemoveTexture(it->second.texture_id);
    }
    cache_.erase(it);
  }
  pending_.erase(handle);
}

void ThumbnailCache::RemoveStale() {
  for (auto it = cache_.begin(); it != cache_.end();) {
    if (!Engine::asset_manager().HasAsset(it->first)) {
      if (it->second.texture_id) {
        ImGui_ImplVulkan_RemoveTexture(it->second.texture_id);
      }
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
}

void ThumbnailCache::Clear() {
  for (auto& [handle, entry] : cache_) {
    if (entry.texture_id) {
      ImGui_ImplVulkan_RemoveTexture(entry.texture_id);
    }
  }
  cache_.clear();
  pending_.clear();
}

}  // namespace wiesel