
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "util/w_utils.h"
#include "w_buffer.h"
#include "w_mesh.h"

namespace Wiesel {
	class Renderer {
	public:
		Renderer();

		static void Create();

		SharedPtr<VertexBuffer> CreateVertexBuffer(const Mesh& mesh);

		WIESEL_GETTER_FN SharedPtr<Renderer> GetRenderer();
	private:
		static SharedPtr<Renderer> s_Renderer;

	};
}