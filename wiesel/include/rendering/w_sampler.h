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
// Created by Metehan Gezer on 24/04/2025.
//

#ifndef WIESEL_SAMPLER_H
#define WIESEL_SAMPLER_H

#include "w_pch.h"

namespace wiesel {

// * VK_SAMPLER_ADDRESS_MODE_REPEAT: Repeat the texture when going beyond the image dimensions.
// * VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: Like repeat, but inverts the coordinates to mirror the image when going beyond the dimensions.
// * VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: Take the color of the edge closest to the coordinate beyond the image dimensions.
// * VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: Like clamp to edge, but instead uses the edge opposite to the closest edge.
// * VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: Return a solid color when sampling beyond the dimensions of the image.
struct SamplerProps {
  VkFilter mag_filter = VK_FILTER_LINEAR;
  VkFilter min_filter = VK_FILTER_LINEAR;
  float max_anisotropy = -1.0f;
  VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkBorderColor border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  VkBool32 compare_enable = VK_FALSE;
  VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS;
};

class Sampler {
 public:
  Sampler(uint32_t mip_levels, const SamplerProps& props);
  ~Sampler();

  VkSampler handle() const { return handle_; }

 private:
  friend class Renderer;
  friend class DescriptorSet;

  VkSampler handle_;
  uint32_t mip_levels_;
  VkFilter mag_filter_;
  VkFilter min_filter_;
  float max_anisotropy_;
  VkSamplerAddressMode address_mode_;
  VkBorderColor border_color_;
};
}  // namespace wiesel
#endif  //WIESEL_SAMPLER_H
