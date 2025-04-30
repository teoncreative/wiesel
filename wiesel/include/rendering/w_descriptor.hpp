
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "rendering/w_sampler.hpp"
#include "util/w_utils.hpp"
#include "w_image.hpp"
#include "w_pch.hpp"

namespace Wiesel {
class UniformBuffer;
class ImageView;
class DescriptorSetLayout;

class DescriptorSet {
 public:
  DescriptorSet();
  ~DescriptorSet();

  void SetLayout(Ref<DescriptorSetLayout> layout) {
    m_Layout = layout;
  }

  void AddCombinedImageSampler(uint32_t dstBinding, Ref<ImageView> view, Ref<Sampler> sampler) {
    m_CombinedImageSamplers.push_back({
        .DstBinding = dstBinding,
        .ImageView = view,
        .Sampler = sampler
    });
  }

  void AddUniformBuffer(uint32_t dstBinding, Ref<UniformBuffer> ubo) {
    m_UniformBufferData.push_back({
        .DstBinding = dstBinding,
        .Ubo = ubo
    });
  }

  void Bake();

  bool m_Allocated;
  VkDescriptorPool m_DescriptorPool;
  VkDescriptorSet m_DescriptorSet;
 private:
  Ref<DescriptorSetLayout> m_Layout;
  struct CombinedImageSamplerData {
    uint32_t DstBinding;
    Ref<ImageView> ImageView;
    Ref<Sampler> Sampler;
  };
  struct UniformBufferData {
    uint32_t DstBinding;
    Ref<UniformBuffer> Ubo;
  };
  std::vector<CombinedImageSamplerData> m_CombinedImageSamplers;
  std::vector<UniformBufferData> m_UniformBufferData;
};
}  // namespace Wiesel