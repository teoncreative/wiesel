
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

namespace Wiesel {
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
  VkBuffer buffer_handle_;
  VkDeviceMemory memory_handle_;
  uint32_t size_;
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

  void* data_;
};

}  // namespace Wiesel