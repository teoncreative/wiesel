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

#include "rendering/w_sampler.h"
#include "w_engine.h"
#include "w_pch.h"

namespace Wiesel {

Sampler::Sampler(uint32_t mipLevels, const Wiesel::SamplerProps& props) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = props.MagFilter;
  samplerInfo.minFilter = props.MinFilter;
  samplerInfo.addressModeU = props.AddressMode;
  samplerInfo.addressModeV = props.AddressMode;
  samplerInfo.addressModeW = props.AddressMode;

  const VkPhysicalDeviceProperties& properties =
      Engine::renderer()->GetPhysicalDeviceProperties();

  if (props.MaxAnisotropy <= 0) {
    samplerInfo.anisotropyEnable = VK_FALSE;
  } else {
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy =
        std::min(props.MaxAnisotropy, properties.limits.maxSamplerAnisotropy);
  }
  samplerInfo.borderColor = props.BorderColor;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = props.CompareEnable;
  samplerInfo.compareOp = props.CompareOp;

  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.maxLod = static_cast<float>(mipLevels);

  WIESEL_CHECK_VKRESULT(vkCreateSampler(Engine::renderer()->GetLogicalDevice(),
                                        &samplerInfo, nullptr, &handle_));
}

Sampler::~Sampler() {
  vkDestroySampler(Engine::renderer()->GetLogicalDevice(), handle_, nullptr);
}

}  // namespace Wiesel