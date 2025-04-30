
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_buffer.hpp"

#include "w_engine.hpp"

namespace Wiesel {

MemoryBuffer::MemoryBuffer(MemoryType type) : m_Type(type) {}

MemoryBuffer::~MemoryBuffer() {
  switch (m_Type) {
    case MemoryTypeVertexBuffer:
      Engine::GetRenderer()->DestroyVertexBuffer(*this);
      break;
    case MemoryTypeIndexBuffer:
      Engine::GetRenderer()->DestroyIndexBuffer(*this);
      break;
    case MemoryTypeUniformBuffer:
      // this is handled by the object
      break;
  }
}

UniformBuffer::UniformBuffer() : MemoryBuffer(MemoryTypeUniformBuffer) {

}

UniformBuffer::~UniformBuffer() {
  Engine::GetRenderer()->DestroyUniformBuffer(*this);
}

}  // namespace Wiesel
