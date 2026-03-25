
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_buffer.h"

#include "w_engine.h"

namespace Wiesel {

MemoryBuffer::MemoryBuffer(MemoryType type) : type_(type) {}

MemoryBuffer::~MemoryBuffer() {
  if (type_ == MemoryTypeUniformBuffer) {
    return;
  }
  if (!buffer_handle_) {
    return;
  }
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }

  VkBuffer buffer = buffer_handle_;
  VkDeviceMemory memory = memory_handle_;
  buffer_handle_ = VK_NULL_HANDLE;
  memory_handle_ = VK_NULL_HANDLE;

  renderer->GetDeletionQueue().Push([renderer, buffer, memory]() {
    VkDevice device = renderer->GetLogicalDevice();
    if (buffer) {
      vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory) {
      vkFreeMemory(device, memory, nullptr);
    }
  });
}

UniformBuffer::UniformBuffer() : MemoryBuffer(MemoryTypeUniformBuffer) {}

UniformBuffer::~UniformBuffer() {
  if (!buffer_handle_) {
    return;
  }
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }

  VkBuffer buffer = buffer_handle_;
  VkDeviceMemory memory = memory_handle_;
  buffer_handle_ = VK_NULL_HANDLE;
  memory_handle_ = VK_NULL_HANDLE;

  renderer->GetDeletionQueue().Push([renderer, buffer, memory]() {
    VkDevice device = renderer->GetLogicalDevice();
    if (buffer) {
      vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory) {
      vkFreeMemory(device, memory, nullptr);
    }
  });
}

}  // namespace Wiesel
