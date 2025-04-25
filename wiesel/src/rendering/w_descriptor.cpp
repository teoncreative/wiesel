
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
    m_Allocated = false;
}

DescriptorSet::~DescriptorSet() {
  if (!m_Allocated) {
    return;
  }
  vkDestroyDescriptorPool(Engine::GetRenderer()->GetLogicalDevice(), m_DescriptorPool,
                          nullptr);
}

void DescriptorSet::Bake() {
  if (m_Allocated) {
    // Destroying the pool is enough to destroy all descriptor set objects.
    vkDestroyDescriptorPool(Engine::GetRenderer()->GetLogicalDevice(), m_DescriptorPool,
                            nullptr);
    m_Allocated = false;
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
      Engine::GetRenderer()->GetLogicalDevice(), &poolInfo, nullptr, &m_DescriptorPool));

  std::vector<VkDescriptorSetLayout> layouts{
      1, m_Layout->m_Layout};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_DescriptorPool;
  allocInfo.descriptorSetCount = layouts.size();
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(Engine::GetRenderer()->GetLogicalDevice(), &allocInfo,
                                                 &m_DescriptorSet));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(m_CombinedImageSamplers.size() + m_UniformBufferData.size());
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(m_UniformBufferData.size());
  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(m_CombinedImageSamplers.size());

  for (const auto& item : m_CombinedImageSamplers) {
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = item.ImageView->m_Handle;
    imageInfo.sampler = item.Sampler->m_Sampler;
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = m_DescriptorSet;
    set.dstBinding = item.DstBinding;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  for (const auto& item : m_UniformBufferData) {
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = item.Ubo->m_Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = item.Ubo->m_Size;
    bufferInfos.emplace_back(bufferInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = m_DescriptorSet;
    set.dstBinding = item.DstBinding;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &bufferInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }
  vkUpdateDescriptorSets(Engine::GetRenderer()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  m_Allocated = true;
}

}  // namespace Wiesel