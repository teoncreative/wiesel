//
// Created by Metehan Gezer on 24/04/2025.
//

#include "rendering/w_sampler.hpp"
#include "w_engine.hpp"
#include "w_pch.hpp"

namespace Wiesel {

Sampler::Sampler(uint32_t mipLevels, const Wiesel::SamplerProps& props) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = props.MagFilter;
  samplerInfo.minFilter = props.MinFilter;
  samplerInfo.addressModeU = props.AddressMode;
  samplerInfo.addressModeV = props.AddressMode;
  samplerInfo.addressModeW = props.AddressMode;

  const VkPhysicalDeviceProperties& properties = Engine::GetRenderer()->GetPhysicalDeviceProperties();

  if (props.MaxAnisotropy <= 0) {
    samplerInfo.anisotropyEnable = VK_FALSE;
  } else {
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = std::min(
        props.MaxAnisotropy, properties.limits.maxSamplerAnisotropy);
  }
  samplerInfo.borderColor = props.BorderColor;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.maxLod = static_cast<float>(mipLevels);

  WIESEL_CHECK_VKRESULT(
      vkCreateSampler(Engine::GetRenderer()->GetLogicalDevice(), &samplerInfo, nullptr, &sampler_));
}

Sampler::~Sampler() {
  vkDestroySampler(Engine::GetRenderer()->GetLogicalDevice(), sampler_, nullptr);
}

}