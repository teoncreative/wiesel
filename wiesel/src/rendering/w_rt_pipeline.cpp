
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_rt_pipeline.hpp"
#include "rendering/w_renderer.hpp"

namespace Wiesel {

RTPipeline::RTPipeline(Ref<Renderer> renderer)
    : renderer_(std::move(renderer)) {}

RTPipeline::~RTPipeline() {
  if (!is_allocated_) return;
  VkDevice device = renderer_->GetLogicalDevice();
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, pipeline_, nullptr);
  }
  if (layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, layout_, nullptr);
  }
  if (sbt_buffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, sbt_buffer_, nullptr);
    vkFreeMemory(device, sbt_memory_, nullptr);
  }
}

void RTPipeline::AddRayGenShader(Ref<Shader> shader) {
  raygen_shaders_.push_back(std::move(shader));
}

void RTPipeline::AddMissShader(Ref<Shader> shader) {
  miss_shaders_.push_back(std::move(shader));
}

void RTPipeline::AddHitGroup(Ref<Shader> closest_hit, Ref<Shader> any_hit,
                             Ref<Shader> intersection) {
  hit_groups_.push_back({std::move(closest_hit), std::move(any_hit),
                         std::move(intersection)});
}

void RTPipeline::AddInputLayout(Ref<DescriptorSetLayout> layout) {
  descriptor_layouts_.push_back(std::move(layout));
}

void RTPipeline::Bake() {
  VkDevice device = renderer_->GetLogicalDevice();

  // Build shader stages and groups
  std::vector<VkPipelineShaderStageCreateInfo> stages;
  std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

  // Ray generation shaders
  for (const auto& shader : raygen_shaders_) {
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = GetShaderFlagBitsByType(shader->properties_.type);
    stage.module = shader->shader_module_;
    stage.pName = shader->properties_.main.c_str();
    stages.push_back(stage);

    VkRayTracingShaderGroupCreateInfoKHR group{};
    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = static_cast<uint32_t>(stages.size() - 1);
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.anyHitShader = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;
    groups.push_back(group);
  }

  // Miss shaders
  for (const auto& shader : miss_shaders_) {
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = GetShaderFlagBitsByType(shader->properties_.type);
    stage.module = shader->shader_module_;
    stage.pName = shader->properties_.main.c_str();
    stages.push_back(stage);

    VkRayTracingShaderGroupCreateInfoKHR group{};
    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = static_cast<uint32_t>(stages.size() - 1);
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.anyHitShader = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;
    groups.push_back(group);
  }

  // Hit groups
  for (const auto& hit : hit_groups_) {
    VkRayTracingShaderGroupCreateInfoKHR group{};
    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.anyHitShader = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;

    if (hit.closest_hit) {
      VkPipelineShaderStageCreateInfo stage{};
      stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
      stage.module = hit.closest_hit->shader_module_;
      stage.pName = hit.closest_hit->properties_.main.c_str();
      stages.push_back(stage);
      group.closestHitShader = static_cast<uint32_t>(stages.size() - 1);
    }

    if (hit.any_hit) {
      VkPipelineShaderStageCreateInfo stage{};
      stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stage.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
      stage.module = hit.any_hit->shader_module_;
      stage.pName = hit.any_hit->properties_.main.c_str();
      stages.push_back(stage);
      group.anyHitShader = static_cast<uint32_t>(stages.size() - 1);
    }

    if (hit.intersection) {
      VkPipelineShaderStageCreateInfo stage{};
      stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stage.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
      stage.module = hit.intersection->shader_module_;
      stage.pName = hit.intersection->properties_.main.c_str();
      stages.push_back(stage);
      group.intersectionShader = static_cast<uint32_t>(stages.size() - 1);
      group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
    }

    groups.push_back(group);
  }

  // Pipeline layout
  std::vector<VkDescriptorSetLayout> layouts;
  layouts.reserve(descriptor_layouts_.size());
  for (const auto& l : descriptor_layouts_) {
    layouts.push_back(l->layout_);
  }

  std::vector<VkPushConstantRange> pushRanges;
  for (const auto& pc : push_constants_) {
    pushRanges.push_back({pc.flags, pc.offset, pc.size});
  }

  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
  layoutInfo.pSetLayouts = layouts.data();
  layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushRanges.size());
  layoutInfo.pPushConstantRanges = pushRanges.data();
  WIESEL_CHECK_VKRESULT(
      vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout_));

  // Create RT pipeline
  VkRayTracingPipelineCreateInfoKHR rtInfo{};
  rtInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
  rtInfo.stageCount = static_cast<uint32_t>(stages.size());
  rtInfo.pStages = stages.data();
  rtInfo.groupCount = static_cast<uint32_t>(groups.size());
  rtInfo.pGroups = groups.data();
  rtInfo.maxPipelineRayRecursionDepth = 1;
  rtInfo.layout = layout_;

  WIESEL_CHECK_VKRESULT(renderer_->vkCreateRayTracingPipelinesKHR()(
      device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rtInfo, nullptr,
      &pipeline_));

  CreateSBT();
  is_allocated_ = true;
}

void RTPipeline::CreateSBT() {
  VkDevice device = renderer_->GetLogicalDevice();
  const auto& props = renderer_->GetRTProperties();

  uint32_t handleSize = props.shaderGroupHandleSize;
  uint32_t handleAlignment = props.shaderGroupHandleAlignment;
  uint32_t baseAlignment = props.shaderGroupBaseAlignment;

  // Align handle size up to handleAlignment
  uint32_t handleSizeAligned =
      (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);

  uint32_t raygenCount = static_cast<uint32_t>(raygen_shaders_.size());
  uint32_t missCount = static_cast<uint32_t>(miss_shaders_.size());
  uint32_t hitCount = static_cast<uint32_t>(hit_groups_.size());
  uint32_t totalGroups = raygenCount + missCount + hitCount;

  // Get shader group handles
  uint32_t handleDataSize = totalGroups * handleSize;
  std::vector<uint8_t> handleData(handleDataSize);
  WIESEL_CHECK_VKRESULT(renderer_->vkGetRayTracingShaderGroupHandlesKHR()(
      device, pipeline_, 0, totalGroups, handleDataSize, handleData.data()));

  // Calculate SBT region sizes (each region aligned to baseAlignment)
  auto alignUp = [](uint32_t value, uint32_t alignment) -> uint32_t {
    return (value + alignment - 1) & ~(alignment - 1);
  };

  uint32_t raygenSize = alignUp(handleSizeAligned * raygenCount, baseAlignment);
  uint32_t missSize = alignUp(handleSizeAligned * missCount, baseAlignment);
  uint32_t hitSize = alignUp(handleSizeAligned * hitCount, baseAlignment);
  uint32_t sbtSize = raygenSize + missSize + hitSize;

  // Create SBT buffer
  renderer_->CreateBuffer(
      sbtSize,
      VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      sbt_buffer_, sbt_memory_);

  // Map and fill
  void* mapped = nullptr;
  vkMapMemory(device, sbt_memory_, 0, sbtSize, 0, &mapped);
  auto* dst = static_cast<uint8_t*>(mapped);
  memset(dst, 0, sbtSize);

  uint32_t handleIdx = 0;
  // Raygen
  for (uint32_t i = 0; i < raygenCount; i++) {
    memcpy(dst + i * handleSizeAligned,
           handleData.data() + handleIdx * handleSize, handleSize);
    handleIdx++;
  }
  // Miss
  for (uint32_t i = 0; i < missCount; i++) {
    memcpy(dst + raygenSize + i * handleSizeAligned,
           handleData.data() + handleIdx * handleSize, handleSize);
    handleIdx++;
  }
  // Hit
  for (uint32_t i = 0; i < hitCount; i++) {
    memcpy(dst + raygenSize + missSize + i * handleSizeAligned,
           handleData.data() + handleIdx * handleSize, handleSize);
    handleIdx++;
  }

  vkUnmapMemory(device, sbt_memory_);

  // Get device address
  VkBufferDeviceAddressInfo addrInfo{};
  addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  addrInfo.buffer = sbt_buffer_;
  VkDeviceAddress sbtAddr = vkGetBufferDeviceAddress(device, &addrInfo);

  // Set up strided regions
  // Raygen: spec requires size == stride (only one raygen shader invoked)
  raygen_region_.deviceAddress = sbtAddr;
  raygen_region_.stride = handleSizeAligned;
  raygen_region_.size = handleSizeAligned;

  miss_region_.deviceAddress = sbtAddr + raygenSize;
  miss_region_.stride = handleSizeAligned;
  miss_region_.size = missSize;

  hit_region_.deviceAddress = sbtAddr + raygenSize + missSize;
  hit_region_.stride = handleSizeAligned;
  hit_region_.size = hitSize;

  callable_region_ = {};  // Not used
}

void RTPipeline::Bind(VkCommandBuffer cmd) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_);

  // Push constants
  for (const auto& pc : push_constants_) {
    vkCmdPushConstants(cmd, layout_, pc.flags, pc.offset, pc.size, pc.ptr.get());
  }
}

void RTPipeline::BindDescriptorSet(VkCommandBuffer cmd, VkDescriptorSet set,
                                   uint32_t index) {
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                          layout_, index, 1, &set, 0, nullptr);
}

}  // namespace Wiesel