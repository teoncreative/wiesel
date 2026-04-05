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

#include "w_pch.h"

struct VmaAllocator_T;
struct VmaAllocation_T;
typedef VmaAllocator_T* VmaAllocator;
typedef VmaAllocation_T* VmaAllocation;

namespace Wiesel {

// RAII wrapper for a VMA-allocated Vulkan buffer.
// Non-copyable, non-movable. As long as this object is alive,
// the underlying VkBuffer and VmaAllocation are valid.
// Destructor calls vmaDestroyBuffer.
class VmaBuffer {
 public:
  VmaBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation,
            const std::string& debug_name = "")
      : allocator_(allocator), buffer_(buffer), allocation_(allocation) {
    SetDebugName(debug_name);
  }

  ~VmaBuffer();

  VmaBuffer(const VmaBuffer&) = delete;
  VmaBuffer& operator=(const VmaBuffer&) = delete;
  VmaBuffer(VmaBuffer&&) = delete;
  VmaBuffer& operator=(VmaBuffer&&) = delete;

  VkBuffer Handle() const { return buffer_; }

  VmaAllocation Allocation() const { return allocation_; }

  VmaAllocator Allocator() const { return allocator_; }

  void SetDebugName(const std::string& name);

 private:
  VmaAllocator allocator_;
  VkBuffer buffer_;
  VmaAllocation allocation_;
};

// RAII wrapper for a VMA-allocated Vulkan image.
// Non-copyable, non-movable. Same semantics as VmaBuffer.
class VmaImage {
 public:
  VmaImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation,
           const std::string& debug_name = "")
      : allocator_(allocator), image_(image), allocation_(allocation) {
    SetDebugName(debug_name);
  }

  ~VmaImage();

  VmaImage(const VmaImage&) = delete;
  VmaImage& operator=(const VmaImage&) = delete;
  VmaImage(VmaImage&&) = delete;
  VmaImage& operator=(VmaImage&&) = delete;

  VkImage Handle() const { return image_; }

  VmaAllocation Allocation() const { return allocation_; }

  void SetDebugName(const std::string& name);

 private:
  VmaAllocator allocator_;
  VkImage image_;
  VmaAllocation allocation_;
};

}  // namespace Wiesel
