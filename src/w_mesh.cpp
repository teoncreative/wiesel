
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_mesh.h"
#include "w_renderer.h"

namespace Wiesel {
	Mesh::Mesh(const glm::vec3& position, const glm::quat& orientation) : Object(position, orientation) {

	}

	Mesh::Mesh(const glm::vec3& position, const glm::quat& orientation, std::vector<Vertex> vertices, std::vector<Index> indices): Mesh(position, orientation) {
		m_Vertices = vertices;
		m_Indices = indices;
		m_TexturePath = "";
		m_Texture = nullptr;
	}

	Mesh::~Mesh() {
		Deallocate();
		Wiesel::LogDebug("Destroying mesh");
	}

	void Mesh::AddVertex(Vertex vertex) {
		m_Vertices.push_back(vertex);
	}

	void Mesh::AddIndex(Index index) {
		m_Indices.push_back(index);
	}

	void Mesh::Allocate() {
		if (m_Allocated) {
			Deallocate();
		}
		m_VertexBuffer = Renderer::GetRenderer()->CreateVertexBuffer(m_Vertices);
		m_IndexBuffer = Renderer::GetRenderer()->CreateIndexBuffer(m_Indices);
		m_UniformBufferSet = Renderer::GetRenderer()->CreateUniformBufferSet(Renderer::k_MaxFramesInFlight);
		if (!m_TexturePath.empty()) {
			m_Texture = Renderer::GetRenderer()->CreateTexture(m_TexturePath);
		}
		m_Descriptors = Renderer::GetRenderer()->CreateDescriptors(m_UniformBufferSet, m_Texture);
		m_Allocated = true;
	}

	void Mesh::Deallocate() {
		if (!m_Allocated) {
			return;
		}
		m_Texture = nullptr;
		m_UniformBufferSet = nullptr;
		m_Descriptors = nullptr;
		m_VertexBuffer = nullptr;
		m_IndexBuffer = nullptr;
		m_Allocated = false;
	}

	void Mesh::SetTexture(const std::string& path) {
		m_TexturePath = path;
		if (!m_Allocated) {
			return;
		}
		if (!path.empty()) {
			m_Texture = Renderer::GetRenderer()->CreateTexture(path);
		} else {
			m_Texture = nullptr;
		}
		m_Descriptors = Renderer::GetRenderer()->CreateDescriptors(m_UniformBufferSet, m_Texture);
	}

	bool Mesh::IsAllocated() const {
		return m_Allocated;
	}

	Reference<MemoryBuffer> Mesh::GetVertexBuffer() {
		return m_VertexBuffer;
	}

	Reference<MemoryBuffer> Mesh::GetIndexBuffer() {
		return m_IndexBuffer;
	}

	Reference<UniformBufferSet> Mesh::GetUniformBufferSet() {
		return m_UniformBufferSet;
	}

	Reference<Texture> Mesh::GetTexture() {
		return m_Texture;
	}

	Reference<DescriptorPool> Mesh::GetDescriptors() {
		return m_Descriptors;
	}

	void Mesh::UpdateUniformBuffer() {
		if (!m_Allocated) {
			return;
		}

		Reference<Camera> camera = Renderer::GetRenderer()->GetActiveCamera();
		Wiesel::UniformBufferObject ubo{};
		ubo.Model = m_LocalView;
		ubo.View = glm::inverse(camera->GetLocalView());
		ubo.Proj = camera->GetProjection();

		uint32_t currentFrame = Renderer::GetRenderer()->GetCurrentFrame();
		memcpy(m_UniformBufferSet->m_Buffers[currentFrame]->m_Data, &ubo, sizeof(ubo));
	}

	std::vector<Vertex> Mesh::GetVertices() {
		return m_Vertices;
	}

	std::vector<Index> Mesh::GetIndices() {
		return m_Indices;
	}

}