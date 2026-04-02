
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

#include <vk_mem_alloc.h>

#include "w_engine.h"

namespace Wiesel {

MemoryBuffer::MemoryBuffer(MemoryType type) : type_(type) {}

MemoryBuffer::~MemoryBuffer() {
  if (type_ == MemoryTypeUniformBuffer) {
    return;
  }
  if (!vma_buffer_) {
    return;
  }
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }

  buffer_handle_ = VK_NULL_HANDLE;
  renderer->GetDeletionQueue().Defer(std::move(vma_buffer_));
}

UniformBuffer::UniformBuffer() : MemoryBuffer(MemoryTypeUniformBuffer) {}

UniformBuffer::~UniformBuffer() {
  if (!vma_buffer_) {
    return;
  }
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }

  // Must unmap before VMA destroys the allocation
  if (data_) {
    vmaUnmapMemory(renderer->GetAllocator(), vma_buffer_->Allocation());
    data_ = nullptr;
  }

  buffer_handle_ = VK_NULL_HANDLE;
  renderer->GetDeletionQueue().Defer(std::move(vma_buffer_));
}

}  // namespace Wiesel
