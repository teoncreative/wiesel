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
};

class Sampler {
 public:
  Sampler(uint32_t mipLevels, const SamplerProps& props);
  ~Sampler();

 private:
  friend class Renderer;
  friend class DescriptorSet;

  VkSampler m_Sampler;
  uint32_t m_MipLevels;
  VkFilter m_MagFilter;
  VkFilter m_MinFilter;
  float m_MaxAnisotropy;
  VkSamplerAddressMode m_AddressMode;
  VkBorderColor m_BorderColor;
};
}
#endif  //WIESEL_SAMPLER_H
