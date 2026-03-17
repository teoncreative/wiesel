
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_acceleration_structure.hpp"
#include "rendering/w_deletion_queue.hpp"
#include "rendering/w_renderer.hpp"
#include "scene/w_scene.hpp"
#include "asset/w_asset_manager.hpp"

namespace Wiesel {

AccelerationStructureManager::AccelerationStructureManager(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {}

AccelerationStructureManager::~AccelerationStructureManager() {
  VkDevice device = renderer_->GetLogicalDevice();

  for (auto& [_, blas] : blas_cache_) {
    DestroyAS(*blas);
  }
  blas_cache_.clear();

  if (tlas_) {
    DestroyAS(*tlas_);
  }

  if (tlas_instance_buffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, tlas_instance_buffer_, nullptr);
    vkFreeMemory(device, tlas_instance_memory_, nullptr);
  }

  if (tlas_scratch_buffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, tlas_scratch_buffer_, nullptr);
    vkFreeMemory(device, tlas_scratch_memory_, nullptr);
  }
}

void AccelerationStructureManager::DestroyAS(AccelerationStructure& as) {
  VkDevice device = renderer_->GetLogicalDevice();
  if (as.handle != VK_NULL_HANDLE) {
    renderer_->vkDestroyAccelerationStructureKHR()(device, as.handle, nullptr);
    as.handle = VK_NULL_HANDLE;
  }
  if (as.buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, as.buffer, nullptr);
    vkFreeMemory(device, as.memory, nullptr);
    as.buffer = VK_NULL_HANDLE;
    as.memory = VK_NULL_HANDLE;
  }
}

VkBuffer AccelerationStructureManager::CreateScratchBuffer(
    VkDeviceSize size, VkDeviceMemory& memory) {
  VkBuffer buffer;
  renderer_->CreateBuffer(
      size,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);
  return buffer;
}

std::shared_ptr<AccelerationStructure> AccelerationStructureManager::GetOrBuildBLAS(
    std::shared_ptr<Mesh> mesh) {
  auto it = blas_cache_.find(mesh.get());
  if (it != blas_cache_.end()) {
    return it->second;
  }

  if (!mesh->allocated_ || !mesh->vertex_buffer || !mesh->index_buffer) {
    return nullptr;
  }

  VkDevice device = renderer_->GetLogicalDevice();

  // Geometry description
  VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
  triangles.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
  triangles.vertexData.deviceAddress = mesh->vertex_buffer->device_address_;
  triangles.vertexStride = sizeof(Vertex3D);
  triangles.maxVertex = mesh->vertices.size() > 0
                            ? static_cast<uint32_t>(mesh->vertices.size() - 1)
                            : 0;
  triangles.indexType = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress = mesh->index_buffer->device_address_;

  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  geometry.geometry.triangles = triangles;

  uint32_t primitiveCount =
      static_cast<uint32_t>(mesh->indices.size()) / 3;

  // Query build sizes
  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
  buildInfo.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  buildInfo.flags =
      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
  buildInfo.geometryCount = 1;
  buildInfo.pGeometries = &geometry;

  VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
  sizeInfo.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
  renderer_->vkGetAccelerationStructureBuildSizesKHR()(
      device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
      &primitiveCount, &sizeInfo);

  // Create AS buffer
  auto blas = std::make_shared<AccelerationStructure>();
  renderer_->CreateBuffer(
      sizeInfo.accelerationStructureSize,
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blas->buffer, blas->memory);

  // Create AS
  VkAccelerationStructureCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  createInfo.buffer = blas->buffer;
  createInfo.size = sizeInfo.accelerationStructureSize;
  createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  WIESEL_CHECK_VKRESULT(renderer_->vkCreateAccelerationStructureKHR()(
      device, &createInfo, nullptr, &blas->handle));

  // Create scratch buffer
  VkDeviceMemory scratchMemory;
  VkBuffer scratchBuffer =
      CreateScratchBuffer(sizeInfo.buildScratchSize, scratchMemory);

  VkBufferDeviceAddressInfo scratchAddrInfo{};
  scratchAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  scratchAddrInfo.buffer = scratchBuffer;
  VkDeviceAddress scratchAddr =
      vkGetBufferDeviceAddress(device, &scratchAddrInfo);

  // Build
  buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  buildInfo.dstAccelerationStructure = blas->handle;
  buildInfo.scratchData.deviceAddress = scratchAddr;

  VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
  rangeInfo.primitiveCount = primitiveCount;
  const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

  // Single-time command for BLAS build
  VkCommandBuffer cmd = renderer_->BeginSingleTimeCommands();
  renderer_->vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &buildInfo,
                                                   &pRangeInfo);
  renderer_->EndSingleTimeCommands(cmd);

  // Get device address
  VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
  addrInfo.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  addrInfo.accelerationStructure = blas->handle;
  blas->device_address =
      renderer_->vkGetAccelerationStructureDeviceAddressKHR()(device,
                                                              &addrInfo);

  // Cleanup scratch
  vkDestroyBuffer(device, scratchBuffer, nullptr);
  vkFreeMemory(device, scratchMemory, nullptr);

  blas_cache_[mesh.get()] = blas;
  return blas;
}

void AccelerationStructureManager::BuildTLAS(VkCommandBuffer cmd,
                                             Scene& scene) {
  VkDevice device = renderer_->GetLogicalDevice();
  AssetManager& assets = Engine::asset_manager();

  // Collect instances
  std::vector<VkAccelerationStructureInstanceKHR> instances;

  for (const auto& entity :
       scene.GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
    auto& model = scene.GetComponent<ModelComponent>(entity);
    auto& transform = scene.GetComponent<TransformComponent>(entity);

    if (!model.enable_rendering || !model.model_handle) continue;

    const std::shared_ptr<Model>& modelData = assets.GetOrLoad<Model>(model.model_handle);
    if (!modelData) continue;

    for (size_t mi = 0; mi < modelData->meshes.size(); mi++) {
      const auto& mesh = modelData->meshes[mi];
      if (!mesh->allocated_) continue;

      std::shared_ptr<AccelerationStructure> blas = GetOrBuildBLAS(mesh);
      if (!blas) continue;

      // Apply per-mesh node transform for static models
      glm::mat4 mesh_world = transform.transform_matrix;
      if (!modelData->has_skeleton && mi < modelData->mesh_node_transforms.size()) {
        mesh_world = mesh_world * modelData->mesh_node_transforms[mi];
      }

      // Convert glm::mat4 to VkTransformMatrixKHR (3x4 row-major)
      VkTransformMatrixKHR transformMatrix{};
      glm::mat4 t = glm::transpose(mesh_world);
      memcpy(&transformMatrix, &t, sizeof(VkTransformMatrixKHR));

      VkAccelerationStructureInstanceKHR instance{};
      instance.transform = transformMatrix;
      instance.instanceCustomIndex = 0;
      instance.mask = 0xFF;
      instance.instanceShaderBindingTableRecordOffset = 0;
      instance.flags =
          VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
      instance.accelerationStructureReference = blas->device_address;
      instances.push_back(instance);
    }
  }

  if (instances.empty()) {
    // No geometry to trace against, defer destruction of existing TLAS
    if (tlas_ && tlas_->handle != VK_NULL_HANDLE) {
      std::shared_ptr<AccelerationStructure> old_tlas = tlas_;
      std::shared_ptr<Renderer> renderer_ref = renderer_;
      renderer_->GetDeletionQueue().Push([old_tlas, renderer_ref]() {
        VkDevice device = renderer_ref->GetLogicalDevice();
        if (old_tlas->handle != VK_NULL_HANDLE) {
          renderer_ref->vkDestroyAccelerationStructureKHR()(device, old_tlas->handle, nullptr);
        }
        if (old_tlas->buffer != VK_NULL_HANDLE) {
          vkDestroyBuffer(device, old_tlas->buffer, nullptr);
          vkFreeMemory(device, old_tlas->memory, nullptr);
        }
      });
    }
    tlas_ = nullptr;
    return;
  }


  // Ensure instance buffer is large enough
  VkDeviceSize instancesSize =
      sizeof(VkAccelerationStructureInstanceKHR) * instances.size();
  if (instances.size() > tlas_instance_capacity_) {
    if (tlas_instance_buffer_ != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, tlas_instance_buffer_, nullptr);
      vkFreeMemory(device, tlas_instance_memory_, nullptr);
    }
    renderer_->CreateBuffer(
        instancesSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        tlas_instance_buffer_, tlas_instance_memory_);
    tlas_instance_capacity_ = static_cast<uint32_t>(instances.size());
  }

  // Upload instances
  void* mapped = nullptr;
  vkMapMemory(device, tlas_instance_memory_, 0, instancesSize, 0, &mapped);
  memcpy(mapped, instances.data(), instancesSize);
  vkUnmapMemory(device, tlas_instance_memory_);

  VkBufferDeviceAddressInfo instanceAddrInfo{};
  instanceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  instanceAddrInfo.buffer = tlas_instance_buffer_;
  VkDeviceAddress instanceAddr =
      vkGetBufferDeviceAddress(device, &instanceAddrInfo);

  // Geometry for TLAS
  VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
  instancesData.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  instancesData.data.deviceAddress = instanceAddr;

  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geometry.geometry.instances = instancesData;

  uint32_t instanceCount = static_cast<uint32_t>(instances.size());

  // Query build sizes
  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
  buildInfo.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  buildInfo.flags =
      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
  buildInfo.geometryCount = 1;
  buildInfo.pGeometries = &geometry;

  VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
  sizeInfo.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
  renderer_->vkGetAccelerationStructureBuildSizesKHR()(
      device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
      &instanceCount, &sizeInfo);

  // Defer destruction of old TLAS so in-flight frames can still reference it
  if (tlas_ && tlas_->handle != VK_NULL_HANDLE) {
    std::shared_ptr<AccelerationStructure> old_tlas = tlas_;
    std::shared_ptr<Renderer> renderer_ref = renderer_;
    renderer_->GetDeletionQueue().Push([old_tlas, renderer_ref]() {
      VkDevice device = renderer_ref->GetLogicalDevice();
      if (old_tlas->handle != VK_NULL_HANDLE) {
        renderer_ref->vkDestroyAccelerationStructureKHR()(device, old_tlas->handle, nullptr);
      }
      if (old_tlas->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, old_tlas->buffer, nullptr);
        vkFreeMemory(device, old_tlas->memory, nullptr);
      }
    });
  }
  tlas_ = std::make_shared<AccelerationStructure>();

  renderer_->CreateBuffer(
      sizeInfo.accelerationStructureSize,
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlas_->buffer, tlas_->memory);

  VkAccelerationStructureCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  createInfo.buffer = tlas_->buffer;
  createInfo.size = sizeInfo.accelerationStructureSize;
  createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  WIESEL_CHECK_VKRESULT(renderer_->vkCreateAccelerationStructureKHR()(
      device, &createInfo, nullptr, &tlas_->handle));

  // Persistent scratch buffer (grows as needed, survives until next frame)
  if (sizeInfo.buildScratchSize > tlas_scratch_capacity_) {
    if (tlas_scratch_buffer_ != VK_NULL_HANDLE) {
      // Previous frame must be done since we're in a new frame's recording
      vkDestroyBuffer(device, tlas_scratch_buffer_, nullptr);
      vkFreeMemory(device, tlas_scratch_memory_, nullptr);
    }
    tlas_scratch_buffer_ =
        CreateScratchBuffer(sizeInfo.buildScratchSize, tlas_scratch_memory_);
    tlas_scratch_capacity_ = sizeInfo.buildScratchSize;
  }

  VkBufferDeviceAddressInfo scratchAddrInfo{};
  scratchAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  scratchAddrInfo.buffer = tlas_scratch_buffer_;
  VkDeviceAddress scratchAddr =
      vkGetBufferDeviceAddress(device, &scratchAddrInfo);

  // Build TLAS
  buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  buildInfo.dstAccelerationStructure = tlas_->handle;
  buildInfo.scratchData.deviceAddress = scratchAddr;

  VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
  rangeInfo.primitiveCount = instanceCount;
  const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

  renderer_->vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &buildInfo,
                                                   &pRangeInfo);

  // Barrier: AS build -> RT shader read
  VkMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
  barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
  vkCmdPipelineBarrier(
      cmd,
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &barrier, 0,
      nullptr, 0, nullptr);

  // Get TLAS device address
  VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
  addrInfo.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  addrInfo.accelerationStructure = tlas_->handle;
  tlas_->device_address =
      renderer_->vkGetAccelerationStructureDeviceAddressKHR()(device,
                                                              &addrInfo);

}

VkAccelerationStructureKHR AccelerationStructureManager::GetTLAS() const {
  return tlas_ ? tlas_->handle : VK_NULL_HANDLE;
}

}  // namespace Wiesel