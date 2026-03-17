
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

#include "w_pch.hpp"
#include "util/w_utils.hpp"
#include "w_descriptorlayout.hpp"
#include "w_shader.hpp"
#include "w_buffer.hpp"

namespace Wiesel {

class Renderer;

struct RTHitGroup {
  std::shared_ptr<Shader> closest_hit;
  std::shared_ptr<Shader> any_hit;
  std::shared_ptr<Shader> intersection;
};

struct RTPushConstant {
  VkShaderStageFlags flags;
  uint32_t size;
  uint32_t offset;
  std::shared_ptr<void> ptr;
};

class RTPipeline {
 public:
  explicit RTPipeline(std::shared_ptr<Renderer> renderer);
  ~RTPipeline();

  void AddRayGenShader(std::shared_ptr<Shader> shader);
  void AddMissShader(std::shared_ptr<Shader> shader);
  void AddHitGroup(std::shared_ptr<Shader> closest_hit,
                   std::shared_ptr<Shader> any_hit = nullptr,
                   std::shared_ptr<Shader> intersection = nullptr);
  void AddInputLayout(std::shared_ptr<DescriptorSetLayout> layout);

  template <typename T>
  void AddPushConstant(std::shared_ptr<T> ptr, VkShaderStageFlags flags) {
    push_constants_.push_back(RTPushConstant{
        .flags = flags,
        .size = sizeof(T),
        .offset = 0,
        .ptr = std::static_pointer_cast<void>(ptr)});
  }

  void Bake();

  void Bind(VkCommandBuffer cmd);
  void BindDescriptorSet(VkCommandBuffer cmd, VkDescriptorSet set, uint32_t index = 0);

  VkPipelineLayout GetLayout() const { return layout_; }

  const VkStridedDeviceAddressRegionKHR& GetRayGenRegion() const { return raygen_region_; }
  const VkStridedDeviceAddressRegionKHR& GetMissRegion() const { return miss_region_; }
  const VkStridedDeviceAddressRegionKHR& GetHitRegion() const { return hit_region_; }
  const VkStridedDeviceAddressRegionKHR& GetCallableRegion() const { return callable_region_; }

 private:
  void CreateSBT();

  std::shared_ptr<Renderer> renderer_;
  std::vector<std::shared_ptr<Shader>> raygen_shaders_;
  std::vector<std::shared_ptr<Shader>> miss_shaders_;
  std::vector<RTHitGroup> hit_groups_;
  std::vector<std::shared_ptr<DescriptorSetLayout>> descriptor_layouts_;
  std::vector<RTPushConstant> push_constants_;

  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_ = VK_NULL_HANDLE;

  // Shader Binding Table
  VkBuffer sbt_buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory sbt_memory_ = VK_NULL_HANDLE;
  VkStridedDeviceAddressRegionKHR raygen_region_{};
  VkStridedDeviceAddressRegionKHR miss_region_{};
  VkStridedDeviceAddressRegionKHR hit_region_{};
  VkStridedDeviceAddressRegionKHR callable_region_{};

  bool is_allocated_ = false;
};

}  // namespace Wiesel