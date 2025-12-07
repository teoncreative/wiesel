//
// Created by Metehan Gezer on 15/04/2025.
//

#include "rendering/w_framebuffer.hpp"
#include "w_engine.hpp"

namespace Wiesel {

Framebuffer::Framebuffer(std::span<VkImageView> attachments, glm::vec2 extent, RenderPass& render_pass) : extent_(extent) {
  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = render_pass.GetVulkanHandle();
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = extent.x;
  framebufferInfo.height = extent.y;
  framebufferInfo.layers = 1;

  WIESEL_CHECK_VKRESULT(vkCreateFramebuffer(Engine::GetRenderer()->GetLogicalDevice(), &framebufferInfo,
                                            nullptr, &handle_));
}

Framebuffer::~Framebuffer() {
  vkDestroyFramebuffer(Engine::GetRenderer()->GetLogicalDevice(), handle_, nullptr);
}

}