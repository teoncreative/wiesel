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

namespace wiesel {

Sampler::Sampler(uint32_t mip_levels, const SamplerProps& props) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = props.mag_filter;
  samplerInfo.minFilter = props.min_filter;
  samplerInfo.addressModeU = props.address_mode;
  samplerInfo.addressModeV = props.address_mode;
  samplerInfo.addressModeW = props.address_mode;

  const VkPhysicalDeviceProperties& properties =
      Engine::renderer()->GetPhysicalDeviceProperties();

  if (props.max_anisotropy <= 0) {
    samplerInfo.anisotropyEnable = VK_FALSE;
  } else {
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy =
        std::min(props.max_anisotropy, properties.limits.maxSamplerAnisotropy);
  }
  samplerInfo.borderColor = props.border_color;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = props.compare_enable;
  samplerInfo.compareOp = props.compare_op;

  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.maxLod = static_cast<float>(mip_levels);

  WIESEL_CHECK_VKRESULT(vkCreateSampler(Engine::renderer()->GetLogicalDevice(),
                                        &samplerInfo, nullptr, &handle_));
}

Sampler::~Sampler() {
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }
  VkSampler sampler = handle_;
  VkDevice device = renderer->GetLogicalDevice();
  renderer->GetDeletionQueue().Push(
      [device, sampler]() { vkDestroySampler(device, sampler, nullptr); });
}

}  // namespace wiesel