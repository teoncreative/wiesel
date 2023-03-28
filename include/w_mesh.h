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

namespace Wiesel {
	class Mesh : public Object {
	public:

		void AddVertex(Vertex vertex);
		void AddIndex(int index);

		void Allocate();
	private:
		std::vector<Vertex> m_Vertices;
		std::vector<int> m_Indices;
	};
}
