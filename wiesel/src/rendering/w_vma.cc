
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_vma.h"

#include <vk_mem_alloc.h>

#include "util/w_logger.h"

namespace wiesel {

VmaBuffer::~VmaBuffer() {
  vmaDestroyBuffer(allocator_, buffer_, allocation_);
}

void VmaBuffer::SetDebugName(const std::string& name) {
#ifndef NDEBUG
  if (!name.empty() && allocation_) {
    vmaSetAllocationName(allocator_, allocation_, name.c_str());
  }
#endif
}

VmaImage::~VmaImage() {
  vmaDestroyImage(allocator_, image_, allocation_);
}

void VmaImage::SetDebugName(const std::string& name) {
#ifndef NDEBUG
  if (!name.empty() && allocation_) {
    vmaSetAllocationName(allocator_, allocation_, name.c_str());
  }
#endif
}

}  // namespace wiesel