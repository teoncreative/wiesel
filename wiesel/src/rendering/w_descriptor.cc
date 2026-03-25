
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_descriptor.h"
#include "rendering/w_image.h"
#include "rendering/w_sampler.h"
#include "rendering/w_texture.h"

#include "w_engine.h"

namespace Wiesel {

DescriptorSet::DescriptorSet() {
  allocated_ = false;
}

DescriptorSet::~DescriptorSet() {
  if (!allocated_) {
    return;
  }
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }

  VkDescriptorPool pool = descriptor_pool_;
  descriptor_pool_ = VK_NULL_HANDLE;
  allocated_ = false;

  renderer->GetDeletionQueue().Push([renderer, pool]() {
    vkDestroyDescriptorPool(renderer->GetLogicalDevice(), pool, nullptr);
  });
}

void DescriptorSet::Bake() {
  if (allocated_) {
    // Destroying the pool is enough to destroy all descriptor set objects.
    vkDestroyDescriptorPool(Engine::renderer()->GetLogicalDevice(),
                            descriptor_pool_, nullptr);
    allocated_ = false;
  }

  std::vector<VkDescriptorPoolSize> poolSizes;

  if (!combined_image_samplers_.empty()) {
    poolSizes.push_back(
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         static_cast<uint32_t>(combined_image_samplers_.size())});
  }

  if (!uniform_buffer_data_.empty()) {
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         static_cast<uint32_t>(uniform_buffer_data_.size())});
  }

  if (!storage_buffer_data_.empty()) {
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         static_cast<uint32_t>(storage_buffer_data_.size())});
  }

  if (!storage_image_data_.empty()) {
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                         static_cast<uint32_t>(storage_image_data_.size())});
  }

  if (!acceleration_structure_data_.empty()) {
    poolSizes.push_back(
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
         static_cast<uint32_t>(acceleration_structure_data_.size())});
  }

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 1;

  // Allocate pool
  WIESEL_CHECK_VKRESULT(
      vkCreateDescriptorPool(Engine::renderer()->GetLogicalDevice(), &poolInfo,
                             nullptr, &descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{1, layout_->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptor_pool_;
  allocInfo.descriptorSetCount = layouts.size();
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(
      Engine::renderer()->GetLogicalDevice(), &allocInfo, &descriptor_set_));

  size_t total_writes =
      combined_image_samplers_.size() + uniform_buffer_data_.size() +
      storage_buffer_data_.size() + storage_image_data_.size() +
      acceleration_structure_data_.size();
  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(total_writes);
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  buffer_infos.reserve(uniform_buffer_data_.size() +
                       storage_buffer_data_.size());
  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(combined_image_samplers_.size() +
                     storage_image_data_.size());
  std::vector<VkWriteDescriptorSetAccelerationStructureKHR> asWrites;
  asWrites.reserve(acceleration_structure_data_.size());

  for (const auto& item : combined_image_samplers_) {
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = item.image_view->handle_;
    imageInfo.sampler = item.sampler->handle_;
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
    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = item.ubo->buffer_handle_;
    buffer_info.offset = 0;
    buffer_info.range = item.ubo->size_;
    buffer_infos.emplace_back(buffer_info);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = descriptor_set_;
    set.dstBinding = item.dst_binding;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &buffer_infos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  for (const auto& item : storage_buffer_data_) {
    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = item.buffer->buffer_handle_;
    buffer_info.offset = 0;
    buffer_info.range = item.buffer->size_;
    buffer_infos.emplace_back(buffer_info);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = descriptor_set_;
    set.dstBinding = item.dst_binding;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &buffer_infos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  for (const auto& item : storage_image_data_) {
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = item.image_view->handle_;
    imageInfo.sampler = VK_NULL_HANDLE;
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = descriptor_set_;
    set.dstBinding = item.dst_binding;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  for (const auto& item : acceleration_structure_data_) {
    VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
    asWrite.sType =
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asWrite.accelerationStructureCount = 1;
    asWrite.pAccelerationStructures = &item.as;
    asWrites.emplace_back(asWrite);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = descriptor_set_;
    set.dstBinding = item.dst_binding;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    set.descriptorCount = 1;
    set.pNext = &asWrites.back();
    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(Engine::renderer()->GetLogicalDevice(),
                         static_cast<uint32_t>(writes.size()), writes.data(), 0,
                         nullptr);

  allocated_ = true;
}

}  // namespace Wiesel