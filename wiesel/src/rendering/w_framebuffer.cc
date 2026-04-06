//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 15/04/2025.
//

#include "rendering/w_framebuffer.h"
#include "w_engine.h"

namespace Wiesel {

Framebuffer::Framebuffer(std::span<VkImageView> attachments, glm::vec2 extent,
                         RenderPass& render_pass)
    : extent_(extent) {
  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = render_pass.GetVulkanHandle();
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = extent.x;
  framebufferInfo.height = extent.y;
  framebufferInfo.layers = 1;

  WIESEL_CHECK_VKRESULT(
      vkCreateFramebuffer(Engine::renderer()->GetLogicalDevice(),
                          &framebufferInfo, nullptr, &handle_));
}

Framebuffer::~Framebuffer() {
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }
  VkFramebuffer fb = handle_;
  VkDevice device = renderer->GetLogicalDevice();
  renderer->GetDeletionQueue().Push(
      [device, fb]() { vkDestroyFramebuffer(device, fb, nullptr); });
}

}  // namespace Wiesel