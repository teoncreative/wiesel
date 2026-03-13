
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
  if (imgui_descriptor_) {
    ImGui_ImplVulkan_RemoveTexture(imgui_descriptor_);
    imgui_descriptor_ = nullptr;
  }
  Engine::renderer()->DestroyTexture(*this);
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
  Engine::renderer()->DestroyAttachmentTexture(*this);
}

ImageView::~ImageView() {
  vkDestroyImageView(Engine::renderer()->GetLogicalDevice(), handle_, nullptr);
}

}  // namespace Wiesel