

#include "w_buffer.h"
#include "w_renderer.h"

namespace Wiesel {

	MemoryBuffer::MemoryBuffer(MemoryType type) : m_Type(type) {

	}

	MemoryBuffer::~MemoryBuffer() {
		switch (m_Type) {
			case MemoryTypeVertexBuffer:
				Renderer::GetRenderer()->DestroyVertexBuffer(*this);
				break;
			case MemoryTypeIndexBuffer:
				Renderer::GetRenderer()->DestroyIndexBuffer(*this);
				break;
			case MemoryTypeUniformBuffer:
				// this is handled by the object
				break;
		}
	}

	UniformBuffer::UniformBuffer() : MemoryBuffer(MemoryTypeUniformBuffer) {

	}

	UniformBuffer::~UniformBuffer() {
		Renderer::GetRenderer()->DestroyUniformBuffer(*this);
	}
}
