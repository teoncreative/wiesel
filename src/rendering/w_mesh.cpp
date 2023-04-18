
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_mesh.h"
#include "rendering/w_renderer.h"
#include "w_engine.h"

namespace Wiesel {

	Mesh::Mesh() {
		Mat = CreateReference<Material>();
		IsAllocated = false;
	}

	Mesh::Mesh(std::vector<Vertex> vertices, std::vector<Index> indices) {
		Mat = CreateReference<Material>();
		Vertices = vertices;
		Indices = indices;
		IsAllocated = false;
	}

	Mesh::~Mesh() {
		Deallocate();
	}

	void Mesh::UpdateUniformBuffer(TransformComponent& transform) const {
		if (!IsAllocated) {
			return;
		}

		Reference<CameraData> camera = Engine::GetRenderer()->GetCameraData();
		Wiesel::UniformBufferObject ubo{};
		ubo.ModelMatrix = transform.TransformMatrix;
		ubo.Scale = transform.Scale;
		ubo.NormalMatrix = transform.NormalMatrix;
		ubo.RotationMatrix = transform.RotationMatrix;
		ubo.CameraViewMatrix = glm::inverse(camera->ViewMatrix);
		ubo.CameraProjection = camera->Projection;
		ubo.CameraPosition = camera->Position;

		uint32_t currentFrame = Engine::GetRenderer()->GetCurrentFrame();
		memcpy(UniformBufferSet->m_Buffers[currentFrame]->m_Data, &ubo, sizeof(ubo));
	}

	void Mesh::Allocate() {
		if (IsAllocated) {
			Deallocate();
		}

		VertexBuffer = Engine::GetRenderer()->CreateVertexBuffer(Vertices);
		IndexBuffer = Engine::GetRenderer()->CreateIndexBuffer(Indices);
		UniformBufferSet = Engine::GetRenderer()->CreateUniformBufferSet(Renderer::k_MaxFramesInFlight, sizeof(UniformBufferObject));
		Descriptors = Engine::GetRenderer()->CreateDescriptors(UniformBufferSet, Mat);
		IsAllocated = true;
	}

	void Mesh::Deallocate() {
		if (!IsAllocated) {
			return;
		}

		Mat = nullptr;
		UniformBufferSet = nullptr;
		Descriptors = nullptr;
		VertexBuffer = nullptr;
		IndexBuffer = nullptr;
		IsAllocated = false;
	}

}
