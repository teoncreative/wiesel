
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
    layout_ = layout;
  }

  void AddCombinedImageSampler(uint32_t dst_binding, Ref<ImageView> view, Ref<Sampler> sampler) {
    combined_image_samplers_.push_back({
        .dst_binding = dst_binding,
        .image_view = view,
        .sampler = sampler
    });
  }

  void AddUniformBuffer(uint32_t dst_binding, Ref<UniformBuffer> ubo) {
    uniform_buffer_data_.push_back({
        .dst_binding = dst_binding,
        .ubo = ubo
    });
  }

  void Bake();

  bool allocated_;
  VkDescriptorPool descriptor_pool_;
  VkDescriptorSet descriptor_set_;
 private:
  Ref<DescriptorSetLayout> layout_;
  struct CombinedImageSamplerData {
    uint32_t dst_binding;
    Ref<ImageView> image_view;
    Ref<Sampler> sampler;
  };
  struct UniformBufferData {
    uint32_t dst_binding;
    Ref<UniformBuffer> ubo;
  };
  std::vector<CombinedImageSamplerData> combined_image_samplers_;
  std::vector<UniformBufferData> uniform_buffer_data_;
};
}  // namespace Wiesel