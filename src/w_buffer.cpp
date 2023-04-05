

#include "w_buffer.h"
#include "w_renderer.h"
#include "w_engine.h"

namespace Wiesel {

	MemoryBuffer::MemoryBuffer(MemoryType type) : m_Type(type) {

	}

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

	UniformBufferSet::UniformBufferSet(uint32_t buffers) {
		m_Buffers.reserve(buffers);
		m_BufferCount = buffers;
	}

	UniformBufferSet::~UniformBufferSet() {
		Engine::GetRenderer()->DestroyUniformBufferSet(*this);
	}
}
