
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
#include "rendering/w_buffer.hpp"
#include "rendering/w_mesh.hpp"

namespace Wiesel {

class Renderer;
class Scene;

struct AccelerationStructure {
  VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceAddress device_address = 0;
};

class AccelerationStructureManager {
 public:
  explicit AccelerationStructureManager(Ref<Renderer> renderer);
  ~AccelerationStructureManager();

  Ref<AccelerationStructure> GetOrBuildBLAS(Ref<Mesh> mesh);
  void BuildTLAS(VkCommandBuffer cmd, Scene& scene);

  VkAccelerationStructureKHR GetTLAS() const;
  bool HasTLAS() const { return tlas_ != nullptr && tlas_->handle != VK_NULL_HANDLE; }

 private:
  void DestroyAS(AccelerationStructure& as);
  VkBuffer CreateScratchBuffer(VkDeviceSize size, VkDeviceMemory& memory);

  Ref<Renderer> renderer_;
  std::unordered_map<Mesh*, Ref<AccelerationStructure>> blas_cache_;
  Ref<AccelerationStructure> tlas_;

  VkBuffer tlas_instance_buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory tlas_instance_memory_ = VK_NULL_HANDLE;
  uint32_t tlas_instance_capacity_ = 0;

  VkBuffer tlas_scratch_buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory tlas_scratch_memory_ = VK_NULL_HANDLE;
  VkDeviceSize tlas_scratch_capacity_ = 0;
};

}  // namespace Wiesel