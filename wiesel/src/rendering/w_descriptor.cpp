
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_descriptor.hpp"
#include "rendering/w_texture.hpp"
#include "rendering/w_image.hpp"
#include "rendering/w_sampler.hpp"

#include "w_engine.hpp"

namespace Wiesel {

DescriptorSet::DescriptorSet() {
    allocated_ = false;
}

DescriptorSet::~DescriptorSet() {
  if (!allocated_) {
    return;
  }
  vkDestroyDescriptorPool(Engine::GetRenderer()->GetLogicalDevice(), descriptor_pool_,
                          nullptr);
}

void DescriptorSet::Bake() {
  if (allocated_) {
    // Destroying the pool is enough to destroy all descriptor set objects.
    vkDestroyDescriptorPool(Engine::GetRenderer()->GetLogicalDevice(), descriptor_pool_,
                            nullptr);
    allocated_ = false;
  }
  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = std::size(poolSizes);
  poolInfo.pPoolSizes = poolSizes;
  poolInfo.maxSets = 1;

  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      Engine::GetRenderer()->GetLogicalDevice(), &poolInfo, nullptr, &descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, layout_->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptor_pool_;
  allocInfo.descriptorSetCount = layouts.size();
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(Engine::GetRenderer()->GetLogicalDevice(), &allocInfo,
                                                 &descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(combined_image_samplers_.size() + uniform_buffer_data_.size());
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(uniform_buffer_data_.size());
  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(combined_image_samplers_.size());

  for (const auto& item : combined_image_samplers_) {
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = item.image_view->handle_;
    imageInfo.sampler = item.sampler->sampler_;
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = descriptor_set_;
    set.dstBinding = item.dst_binding;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  for (const auto& item : uniform_buffer_data_) {
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = item.ubo->buffer_handle_;
    bufferInfo.offset = 0;
    bufferInfo.range = item.ubo->size_;
    bufferInfos.emplace_back(bufferInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = descriptor_set_;
    set.dstBinding = item.dst_binding;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &bufferInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }
  vkUpdateDescriptorSets(Engine::GetRenderer()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  allocated_ = true;
}

}  // namespace Wiesel