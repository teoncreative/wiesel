
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_acceleration_structure.h"

#include "asset/w_asset_manager.h"
#include "rendering/w_deletion_queue.h"
#include "rendering/w_renderer.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace Wiesel {

AccelerationStructureManager::AccelerationStructureManager(
    std::shared_ptr<Renderer> renderer)
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
  renderer_->CreateBuffer(size,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);
  return buffer;
}

std::shared_ptr<AccelerationStructure>
AccelerationStructureManager::GetOrBuildBLAS(std::shared_ptr<Mesh> mesh) {
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

  uint32_t primitiveCount = static_cast<uint32_t>(mesh->indices.size()) / 3;

  // Query build sizes
  VkAccelerationStructureBuildGeometryInfoKHR build_info{};
  build_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
  build_info.geometryCount = 1;
  build_info.pGeometries = &geometry;

  VkAccelerationStructureBuildSizesInfoKHR size_info{};
  size_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
  renderer_->vkGetAccelerationStructureBuildSizesKHR()(
      device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info,
      &primitiveCount, &size_info);

  // Create AS buffer
  auto blas = std::make_shared<AccelerationStructure>();
  renderer_->CreateBuffer(
      size_info.accelerationStructureSize,
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blas->buffer, blas->memory);

  // Create AS
  VkAccelerationStructureCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  create_info.buffer = blas->buffer;
  create_info.size = size_info.accelerationStructureSize;
  create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  WIESEL_CHECK_VKRESULT(renderer_->vkCreateAccelerationStructureKHR()(
      device, &create_info, nullptr, &blas->handle));

  // Create scratch buffer
  VkDeviceMemory scratch_memory;
  VkBuffer scratch_buffer =
      CreateScratchBuffer(size_info.buildScratchSize, scratch_memory);

  VkBufferDeviceAddressInfo scratch_addr_info{};
  scratch_addr_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  scratch_addr_info.buffer = scratch_buffer;
  VkDeviceAddress scratchAddr =
      vkGetBufferDeviceAddress(device, &scratch_addr_info);

  // Build
  build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  build_info.dstAccelerationStructure = blas->handle;
  build_info.scratchData.deviceAddress = scratchAddr;

  VkAccelerationStructureBuildRangeInfoKHR range_info{};
  range_info.primitiveCount = primitiveCount;
  const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &range_info;

  // Single-time command for BLAS build
  VkCommandBuffer cmd = renderer_->BeginSingleTimeCommands();
  renderer_->vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &build_info,
                                                   &pRangeInfo);
  renderer_->EndSingleTimeCommands(cmd);

  // Get device address
  VkAccelerationStructureDeviceAddressInfoKHR addr_info{};
  addr_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  addr_info.accelerationStructure = blas->handle;
  blas->device_address =
      renderer_->vkGetAccelerationStructureDeviceAddressKHR()(device,
                                                              &addr_info);

  // Cleanup scratch
  vkDestroyBuffer(device, scratch_buffer, nullptr);
  vkFreeMemory(device, scratch_memory, nullptr);

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

    if (!model.enable_rendering || !model.model_handle) {
      continue;
    }

    const std::shared_ptr<Model>& model_data =
        assets.GetOrLoad<Model>(model.model_handle);
    if (!model_data) {
      continue;
    }

    for (size_t mi = 0; mi < model_data->meshes.size(); mi++) {
      const auto& mesh = model_data->meshes[mi];
      if (!mesh->allocated_) {
        continue;
      }

      std::shared_ptr<AccelerationStructure> blas = GetOrBuildBLAS(mesh);
      if (!blas) {
        continue;
      }

      // Apply per-mesh node transform for static models
      glm::mat4 mesh_world = transform.GetTransformMatrix();
      if (!model_data->has_skeleton &&
          mi < model_data->mesh_node_transforms.size()) {
        mesh_world = mesh_world * model_data->mesh_node_transforms[mi];
      }

      // Convert glm::mat4 to VkTransformMatrixKHR (3x4 row-major)
      VkTransformMatrixKHR transform_matrix{};
      glm::mat4 t = glm::transpose(mesh_world);
      memcpy(&transform_matrix, &t, sizeof(VkTransformMatrixKHR));

      VkAccelerationStructureInstanceKHR instance{};
      instance.transform = transform_matrix;
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
          renderer_ref->vkDestroyAccelerationStructureKHR()(
              device, old_tlas->handle, nullptr);
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
  VkDeviceSize instances_size =
      sizeof(VkAccelerationStructureInstanceKHR) * instances.size();
  if (instances.size() > tlas_instance_capacity_) {
    if (tlas_instance_buffer_ != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, tlas_instance_buffer_, nullptr);
      vkFreeMemory(device, tlas_instance_memory_, nullptr);
    }
    renderer_->CreateBuffer(
        instances_size,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        tlas_instance_buffer_, tlas_instance_memory_);
    tlas_instance_capacity_ = static_cast<uint32_t>(instances.size());
  }

  // Upload instances
  void* mapped = nullptr;
  WIESEL_CHECK_VKRESULT(vkMapMemory(device, tlas_instance_memory_, 0,
                                    instances_size, 0, &mapped));
  memcpy(mapped, instances.data(), instances_size);
  vkUnmapMemory(device, tlas_instance_memory_);

  VkBufferDeviceAddressInfo instance_addr_info{};
  instance_addr_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  instance_addr_info.buffer = tlas_instance_buffer_;
  VkDeviceAddress instance_addr =
      vkGetBufferDeviceAddress(device, &instance_addr_info);

  // Geometry for TLAS
  VkAccelerationStructureGeometryInstancesDataKHR instances_data{};
  instances_data.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  instances_data.data.deviceAddress = instance_addr;

  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geometry.geometry.instances = instances_data;

  uint32_t instance_count = static_cast<uint32_t>(instances.size());

  // Query build sizes
  VkAccelerationStructureBuildGeometryInfoKHR build_info{};
  build_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
  build_info.geometryCount = 1;
  build_info.pGeometries = &geometry;

  VkAccelerationStructureBuildSizesInfoKHR size_info{};
  size_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
  renderer_->vkGetAccelerationStructureBuildSizesKHR()(
      device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info,
      &instance_count, &size_info);

  // Defer destruction of old TLAS so in-flight frames can still reference it
  if (tlas_ && tlas_->handle != VK_NULL_HANDLE) {
    std::shared_ptr<AccelerationStructure> old_tlas = tlas_;
    std::shared_ptr<Renderer> renderer_ref = renderer_;
    renderer_->GetDeletionQueue().Push([old_tlas, renderer_ref]() {
      VkDevice device = renderer_ref->GetLogicalDevice();
      if (old_tlas->handle != VK_NULL_HANDLE) {
        renderer_ref->vkDestroyAccelerationStructureKHR()(
            device, old_tlas->handle, nullptr);
      }
      if (old_tlas->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, old_tlas->buffer, nullptr);
        vkFreeMemory(device, old_tlas->memory, nullptr);
      }
    });
  }
  tlas_ = std::make_shared<AccelerationStructure>();

  renderer_->CreateBuffer(
      size_info.accelerationStructureSize,
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlas_->buffer, tlas_->memory);

  VkAccelerationStructureCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  create_info.buffer = tlas_->buffer;
  create_info.size = size_info.accelerationStructureSize;
  create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  WIESEL_CHECK_VKRESULT(renderer_->vkCreateAccelerationStructureKHR()(
      device, &create_info, nullptr, &tlas_->handle));

  // Persistent scratch buffer (grows as needed, survives until next frame)
  if (size_info.buildScratchSize > tlas_scratch_capacity_) {
    if (tlas_scratch_buffer_ != VK_NULL_HANDLE) {
      // Previous frame must be done since we're in a new frame's recording
      vkDestroyBuffer(device, tlas_scratch_buffer_, nullptr);
      vkFreeMemory(device, tlas_scratch_memory_, nullptr);
    }
    tlas_scratch_buffer_ =
        CreateScratchBuffer(size_info.buildScratchSize, tlas_scratch_memory_);
    tlas_scratch_capacity_ = size_info.buildScratchSize;
  }

  VkBufferDeviceAddressInfo scratch_addr_info{};
  scratch_addr_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  scratch_addr_info.buffer = tlas_scratch_buffer_;
  VkDeviceAddress scratchAddr =
      vkGetBufferDeviceAddress(device, &scratch_addr_info);

  // Build TLAS
  build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  build_info.dstAccelerationStructure = tlas_->handle;
  build_info.scratchData.deviceAddress = scratchAddr;

  VkAccelerationStructureBuildRangeInfoKHR range_info{};
  range_info.primitiveCount = instance_count;
  const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &range_info;

  renderer_->vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &build_info,
                                                   &pRangeInfo);

  // Barrier: AS build -> RT shader read
  VkMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
  barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
  vkCmdPipelineBarrier(cmd,
                       VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                       VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1,
                       &barrier, 0, nullptr, 0, nullptr);

  // Get TLAS device address
  VkAccelerationStructureDeviceAddressInfoKHR addr_info{};
  addr_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  addr_info.accelerationStructure = tlas_->handle;
  tlas_->device_address =
      renderer_->vkGetAccelerationStructureDeviceAddressKHR()(device,
                                                              &addr_info);
}

VkAccelerationStructureKHR AccelerationStructureManager::GetTLAS() const {
  return tlas_ ? tlas_->handle : VK_NULL_HANDLE;
}

}  // namespace Wiesel