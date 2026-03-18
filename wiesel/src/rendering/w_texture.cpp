
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_texture.hpp"

#include <backends/imgui_impl_vulkan.h>

#include "w_engine.hpp"

namespace Wiesel {

Texture::Texture(TextureType texture_type, const std::string& path)
    : type_(texture_type), path_(path) {
  width_ = 0;
  height_ = 0;
  size_ = 0;
  is_allocated_ = false;
  mip_levels_ = 1;
}

Texture::~Texture() {
  if (!is_allocated_) return;
  auto renderer = Engine::renderer();
  if (!renderer) return;

  VkImage image = image_;
  VkDeviceMemory memory = image_memory_;
  VkSampler sampler = sampler_;
  VkDescriptorSet imgui_desc = imgui_descriptor_;

  image_ = VK_NULL_HANDLE;
  image_memory_ = VK_NULL_HANDLE;
  sampler_ = VK_NULL_HANDLE;
  imgui_descriptor_ = nullptr;
  // image_view_ shared_ptr will destruct on its own and defer via ImageView::~ImageView

  renderer->GetDeletionQueue().Push([renderer, image, memory, sampler, imgui_desc]() {
    VkDevice device = renderer->GetLogicalDevice();
    if (imgui_desc) {
      ImGui_ImplVulkan_RemoveTexture(imgui_desc);
    }
    if (sampler) {
      vkDestroySampler(device, sampler, nullptr);
    }
    if (image) {
      vkDestroyImage(device, image, nullptr);
    }
    if (memory) {
      vkFreeMemory(device, memory, nullptr);
    }
  });
}

VkDescriptorSet Texture::GetImGuiDescriptor() {
  if (imgui_descriptor_) return imgui_descriptor_;
  if (!is_allocated_ || !image_view_) return nullptr;
  imgui_descriptor_ = ImGui_ImplVulkan_AddTexture(
      sampler_, image_view_->handle_,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  return imgui_descriptor_;
}

AttachmentTexture::~AttachmentTexture() {
  if (!is_allocated_) return;
  auto renderer = Engine::renderer();
  if (!renderer) return;

  // image_views_ contains shared_ptr<ImageView> which self-defer
  auto views = std::move(image_views_);
  auto images = std::move(images_);
  auto memories = std::move(device_memories_);
  auto type = type_;
  is_allocated_ = false;

  renderer->GetDeletionQueue().Push([renderer, views, images, memories, type]() {
    VkDevice device = renderer->GetLogicalDevice();
    // views will be destroyed when this lambda's captures are freed,
    // triggering ImageView::~ImageView which also defers - but that's fine,
    // the inner defer will execute next flush.
    if (type != AttachmentTextureType::SwapChain) {
      for (VkImage image : images) {
        vkDestroyImage(device, image, nullptr);
      }
      for (VkDeviceMemory memory : memories) {
        vkFreeMemory(device, memory, nullptr);
      }
    }
  });
}

ImageView::~ImageView() {
  if (!handle_) return;
  auto renderer = Engine::renderer();
  if (!renderer) return;

  VkImageView view = handle_;
  handle_ = VK_NULL_HANDLE;

  renderer->GetDeletionQueue().Push([renderer, view]() {
    vkDestroyImageView(renderer->GetLogicalDevice(), view, nullptr);
  });
}

}  // namespace Wiesel