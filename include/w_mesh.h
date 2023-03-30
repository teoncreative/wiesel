
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "w_pch.h"
#include "w_object.h"
#include "w_buffer.h"
#include "w_texture.h"
#include "w_descriptor.h"

namespace Wiesel {
	class Mesh : public Object {
	public:
		Mesh(const glm::vec3& position, const glm::quat& orientation);
		Mesh(const glm::vec3& position, const glm::quat& orientation, std::vector<Vertex> vertices, std::vector<Index> indices);
		~Mesh();

		void AddVertex(Vertex vertex);
		void AddIndex(Index index);

		void Allocate();
		void Deallocate();

		void SetTexture(const std::string& path);

		WIESEL_GETTER_FN bool IsAllocated() const;

		WIESEL_GETTER_FN Reference<MemoryBuffer> GetVertexBuffer();
		WIESEL_GETTER_FN Reference<MemoryBuffer> GetIndexBuffer();
		WIESEL_GETTER_FN Reference<UniformBufferSet> GetUniformBufferSet();
		WIESEL_GETTER_FN Reference<Texture> GetTexture();
		WIESEL_GETTER_FN Reference<DescriptorPool> GetDescriptors();

		std::vector<Vertex> GetVertices();
		std::vector<Index> GetIndices();

		void UpdateUniformBuffer();
	private:
		std::vector<Vertex> m_Vertices;
		std::vector<Index> m_Indices;
		bool m_Allocated;
		std::string m_TexturePath;

		Reference<MemoryBuffer> m_VertexBuffer;
		Reference<MemoryBuffer> m_IndexBuffer;
		Reference<UniformBufferSet> m_UniformBufferSet;
		Reference<Texture> m_Texture;
		Reference<DescriptorPool> m_Descriptors;
	};
}