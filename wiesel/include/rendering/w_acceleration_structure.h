
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "rendering/w_buffer.h"
#include "rendering/w_mesh.h"
#include "util/w_utils.h"
#include "w_pch.h"

namespace wiesel {

class Renderer;
class Scene;

struct AccelerationStructure {
  VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VkDeviceAddress device_address = 0;
};

class AccelerationStructureManager {
 public:
  explicit AccelerationStructureManager(std::shared_ptr<Renderer> renderer);
  ~AccelerationStructureManager();

  std::shared_ptr<AccelerationStructure> GetOrBuildBLAS(
      std::shared_ptr<Mesh> mesh);
  void BuildTLAS(VkCommandBuffer cmd, Scene& scene);

  VkAccelerationStructureKHR GetTLAS() const;

  bool HasTLAS() const {
    return tlas_ != nullptr && tlas_->handle != VK_NULL_HANDLE;
  }

 private:
  void DestroyAS(AccelerationStructure& as);
  VkBuffer CreateScratchBuffer(VkDeviceSize size, VmaAllocation& allocation);

  std::shared_ptr<Renderer> renderer_;
  std::unordered_map<Mesh*, std::shared_ptr<AccelerationStructure>> blas_cache_;
  std::shared_ptr<AccelerationStructure> tlas_;

  VkBuffer tlas_instance_buffer_ = VK_NULL_HANDLE;
  VmaAllocation tlas_instance_alloc_ = VK_NULL_HANDLE;
  uint32_t tlas_instance_capacity_ = 0;

  VkBuffer tlas_scratch_buffer_ = VK_NULL_HANDLE;
  VmaAllocation tlas_scratch_alloc_ = VK_NULL_HANDLE;
  VkDeviceSize tlas_scratch_capacity_ = 0;
};

}  // namespace wiesel