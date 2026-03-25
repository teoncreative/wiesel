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

#ifndef WIESEL_FRAMEBUFFER_HPP
#define WIESEL_FRAMEBUFFER_HPP

#include <span>
#include "util/w_utils.h"
#include "w_pch.h"
#include "w_renderpass.h"

namespace Wiesel {
class Framebuffer {
 public:
  Framebuffer(std::span<VkImageView> attachments, glm::vec2 extent,
              RenderPass& render_pass);
  ~Framebuffer();

  glm::vec2 extent_;
  VkFramebuffer handle_;
};
}  // namespace Wiesel
#endif  //WIESEL_W_FRAMEBUFFER_HPP
