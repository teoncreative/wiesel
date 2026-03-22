//
// Created by Metehan Gezer on 24/04/2025.
//

#ifndef WIESEL_SAMPLER_H
#define WIESEL_SAMPLER_H

#include "w_pch.hpp"

namespace Wiesel {

// * VK_SAMPLER_ADDRESS_MODE_REPEAT: Repeat the texture when going beyond the image dimensions.
// * VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: Like repeat, but inverts the coordinates to mirror the image when going beyond the dimensions.
// * VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: Take the color of the edge closest to the coordinate beyond the image dimensions.
// * VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: Like clamp to edge, but instead uses the edge opposite to the closest edge.
// * VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: Return a solid color when sampling beyond the dimensions of the image.
struct SamplerProps {
  VkFilter MagFilter = VK_FILTER_LINEAR;
  VkFilter MinFilter = VK_FILTER_LINEAR;
  float MaxAnisotropy = -1.0f;
  VkSamplerAddressMode AddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkBorderColor BorderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  VkBool32 CompareEnable = VK_FALSE;
  VkCompareOp CompareOp = VK_COMPARE_OP_ALWAYS;
};

class Sampler {
 public:
  Sampler(uint32_t mipLevels, const SamplerProps& props);
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
}
#endif  //WIESEL_SAMPLER_H
