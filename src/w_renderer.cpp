
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_renderer.h"

namespace Wiesel {
	SharedPtr<Renderer> Renderer::s_Renderer;

	Renderer::Renderer() {

	}

	void Renderer::Create() {
		s_Renderer = CreateShared<Renderer>();
	}

	SharedPtr<Renderer> Renderer::GetRenderer() {
		return s_Renderer;
	}

	SharedPtr<VertexBuffer> Renderer::CreateVertexBuffer(const Mesh& mesh) {
		return {};
	}

}