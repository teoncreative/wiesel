
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

#include "rendering/w_vma.h"
#include "w_pch.h"

namespace wiesel {
enum MemoryType {
  MemoryTypeVertexBuffer,
  MemoryTypeIndexBuffer,
  MemoryTypeUniformBuffer
};

class MemoryBuffer {
 public:
  explicit MemoryBuffer(MemoryType type);
  virtual ~MemoryBuffer();

  MemoryType type_;
  VkBuffer buffer_handle_ = VK_NULL_HANDLE;
  std::unique_ptr<VmaBuffer> vma_buffer_;
  uint32_t size_ = 0;
  VkDeviceAddress device_address_ = 0;
};

class IndexBuffer : public MemoryBuffer {
 public:
  IndexBuffer() : MemoryBuffer(MemoryTypeIndexBuffer) {}

  ~IndexBuffer() override {}

  VkIndexType index_type_;
};

class UniformBuffer : public MemoryBuffer {
 public:
  UniformBuffer();
  ~UniformBuffer() override;

  void* data_ = nullptr;
};

}  // namespace wiesel
