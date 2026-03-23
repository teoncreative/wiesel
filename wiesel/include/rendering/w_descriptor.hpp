
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

  void SetLayout(std::shared_ptr<DescriptorSetLayout> layout) {
    layout_ = layout;
  }

  void AddCombinedImageSampler(uint32_t dst_binding,
                               std::shared_ptr<ImageView> view,
                               std::shared_ptr<Sampler> sampler) {
    combined_image_samplers_.push_back(
        {.dst_binding = dst_binding, .image_view = view, .sampler = sampler});
  }

  void AddUniformBuffer(uint32_t dst_binding,
                        std::shared_ptr<UniformBuffer> ubo) {
    uniform_buffer_data_.push_back({.dst_binding = dst_binding, .ubo = ubo});
  }

  void AddStorageBuffer(uint32_t dst_binding,
                        std::shared_ptr<UniformBuffer> buffer) {
    storage_buffer_data_.push_back(
        {.dst_binding = dst_binding, .buffer = buffer});
  }

  void AddStorageImage(uint32_t dst_binding, std::shared_ptr<ImageView> view) {
    storage_image_data_.push_back(
        {.dst_binding = dst_binding, .image_view = view});
  }

  void AddAccelerationStructure(uint32_t dst_binding,
                                VkAccelerationStructureKHR as) {
    acceleration_structure_data_.push_back(
        {.dst_binding = dst_binding, .as = as});
  }

  void ClearBindings() {
    combined_image_samplers_.clear();
    uniform_buffer_data_.clear();
    storage_buffer_data_.clear();
    storage_image_data_.clear();
    acceleration_structure_data_.clear();
  }

  void Bake();

  bool allocated_;
  VkDescriptorPool descriptor_pool_;
  VkDescriptorSet descriptor_set_;

 private:
  std::shared_ptr<DescriptorSetLayout> layout_;

  struct CombinedImageSamplerData {
    uint32_t dst_binding;
    std::shared_ptr<ImageView> image_view;
    std::shared_ptr<Sampler> sampler;
  };

  struct UniformBufferData {
    uint32_t dst_binding;
    std::shared_ptr<UniformBuffer> ubo;
  };

  struct StorageBufferData {
    uint32_t dst_binding;
    std::shared_ptr<UniformBuffer> buffer;
  };

  struct StorageImageData {
    uint32_t dst_binding;
    std::shared_ptr<ImageView> image_view;
  };

  struct AccelerationStructureData {
    uint32_t dst_binding;
    VkAccelerationStructureKHR as;
  };

  std::vector<CombinedImageSamplerData> combined_image_samplers_;
  std::vector<UniformBufferData> uniform_buffer_data_;
  std::vector<StorageBufferData> storage_buffer_data_;
  std::vector<StorageImageData> storage_image_data_;
  std::vector<AccelerationStructureData> acceleration_structure_data_;
};
}  // namespace Wiesel