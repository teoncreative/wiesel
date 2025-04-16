//
// Created by Metehan Gezer on 15/04/2025.
//

#ifndef WIESEL_FRAMEBUFFER_HPP
#define WIESEL_FRAMEBUFFER_HPP

#include <span>
#include "util/w_utils.hpp"
#include "w_pch.hpp"
#include "w_renderpass.hpp"

namespace Wiesel {
class Framebuffer {
 public:
  Framebuffer(std::span<VkImageView> attachments, VkExtent2D extent, RenderPass& renderPass);
  ~Framebuffer();

  VkExtent2D m_Extent;
  VkFramebuffer m_Handle;

};
}
#endif  //WIESEL_W_FRAMEBUFFER_HPP
