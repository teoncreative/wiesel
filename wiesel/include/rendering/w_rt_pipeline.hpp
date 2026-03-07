
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
  Ref<Shader> closest_hit;
  Ref<Shader> any_hit;
  Ref<Shader> intersection;
};

struct RTPushConstant {
  VkShaderStageFlags flags;
  uint32_t size;
  uint32_t offset;
  std::shared_ptr<void> ptr;
};

class RTPipeline {
 public:
  explicit RTPipeline(Ref<Renderer> renderer);
  ~RTPipeline();

  void AddRayGenShader(Ref<Shader> shader);
  void AddMissShader(Ref<Shader> shader);
  void AddHitGroup(Ref<Shader> closest_hit,
                   Ref<Shader> any_hit = nullptr,
                   Ref<Shader> intersection = nullptr);
  void AddInputLayout(Ref<DescriptorSetLayout> layout);

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

  Ref<Renderer> renderer_;
  std::vector<Ref<Shader>> raygen_shaders_;
  std::vector<Ref<Shader>> miss_shaders_;
  std::vector<RTHitGroup> hit_groups_;
  std::vector<Ref<DescriptorSetLayout>> descriptor_layouts_;
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