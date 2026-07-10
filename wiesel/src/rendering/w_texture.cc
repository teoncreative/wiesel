
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_texture.h"

#include <backends/imgui_impl_vulkan.h>

#include "w_engine.h"

namespace wiesel {

// Global texture memory tracking (atomic for thread safety)
std::atomic<uint64_t> g_texture_memory_total{0};
std::atomic<uint32_t> g_texture_count{0};

Texture::Texture(TextureType texture_type, const std::string& path)
    : type_(texture_type), path_(path) {
  width_ = 0;
  height_ = 0;
  size_ = 0;
  is_allocated_ = false;
  mip_levels_ = 1;
}

Texture::~Texture() {
  if (!is_allocated_) {
    return;
  }
  g_texture_memory_total -= size_;
  g_texture_count--;
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }

  VkDescriptorSet imgui_desc = imgui_descriptor_;
  // Capture shared_ptrs so Vulkan resources stay alive until deletion queue fires
  auto sampler = std::move(sampler_);
  auto image_view = std::move(image_view_);

  image_ = VK_NULL_HANDLE;
  imgui_descriptor_ = nullptr;

  renderer->GetDeletionQueue().Defer(std::move(vma_image_));
  renderer->GetDeletionQueue().Push([imgui_desc, sampler, image_view]() {
    if (imgui_desc) {
      ImGui_ImplVulkan_RemoveTexture(imgui_desc);
    }
  });
}

void Texture::MarkAllocated() {
  is_allocated_ = true;
  g_texture_memory_total += size_;
  g_texture_count++;
}

uint64_t Texture::GetTotalTextureMemory() {
  return g_texture_memory_total.load();
}

uint32_t Texture::GetTotalTextureCount() {
  return g_texture_count.load();
}

VkDescriptorSet Texture::GetImGuiDescriptor() {
  if (imgui_descriptor_) {
    return imgui_descriptor_;
  }
  if (!is_allocated_ || !image_view_ || !sampler_) {
    return nullptr;
  }
  imgui_descriptor_ =
      ImGui_ImplVulkan_AddTexture(sampler_->handle(), image_view_->handle_,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  return imgui_descriptor_;
}

AttachmentTexture::~AttachmentTexture() {
  if (!is_allocated_) {
    return;
  }
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }

  is_allocated_ = false;
  images_.clear();

  // Defer image views (shared_ptrs, copyable)
  auto views = std::move(image_views_);
  renderer->GetDeletionQueue().Push([views]() {});

  // Defer VMA images
  for (auto& img : vma_images_) {
    renderer->GetDeletionQueue().Defer(std::move(img));
  }
  vma_images_.clear();
}

ImageView::~ImageView() {
  if (!handle_) {
    return;
  }
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }

  VkImageView view = handle_;
  handle_ = VK_NULL_HANDLE;

  renderer->GetDeletionQueue().Push([renderer, view]() {
    vkDestroyImageView(renderer->GetLogicalDevice(), view, nullptr);
  });
}

}  // namespace wiesel