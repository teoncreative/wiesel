
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_mesh.h"
#include "w_renderer.h"
#include "w_engine.h"

namespace Wiesel {

	Mesh::Mesh() {
		TexturePath = "";
		Texture = nullptr;
		IsAllocated = false;
	}

	Mesh::Mesh(std::vector<Vertex> vertices, std::vector<Index> indices) {
		Vertices = vertices;
		Indices = indices;
		TexturePath = "";
		Texture = nullptr;
		IsAllocated = false;
	}

	Mesh::~Mesh() {
		Deallocate();
	}

	void Mesh::UpdateUniformBuffer(TransformComponent& transform) const {
		if (!IsAllocated) {
			return;
		}

		Reference<Camera> camera = Engine::GetRenderer()->GetActiveCamera();
		Wiesel::UniformBufferObject ubo{};
		ubo.Model = transform.LocalView;
		ubo.View = glm::inverse(camera->GetView());
		ubo.Proj = camera->GetProjection();
		ubo.NormalMatrix = transform.NormalMatrix;

		uint32_t currentFrame = Engine::GetRenderer()->GetCurrentFrame();
		memcpy(UniformBufferSet->m_Buffers[currentFrame]->m_Data, &ubo, sizeof(ubo));
	}

	void Mesh::Allocate() {
		if (IsAllocated) {
			Deallocate();
		}

		VertexBuffer = Engine::GetRenderer()->CreateVertexBuffer(Vertices);
		IndexBuffer = Engine::GetRenderer()->CreateIndexBuffer(Indices);
		UniformBufferSet = Engine::GetRenderer()->CreateUniformBufferSet(Renderer::k_MaxFramesInFlight);
		if (!TexturePath.empty()) {
			Texture = Engine::GetRenderer()->CreateTexture(TexturePath, {});
		}
		Descriptors = Engine::GetRenderer()->CreateDescriptors(UniformBufferSet, Texture);
		IsAllocated = true;
	}

	void Mesh::Deallocate() {
		if (!IsAllocated) {
			return;
		}

		Texture = nullptr;
		UniformBufferSet = nullptr;
		Descriptors = nullptr;
		VertexBuffer = nullptr;
		IndexBuffer = nullptr;
		IsAllocated = false;
	}

}
