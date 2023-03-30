//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_descriptor.h"
#include "w_renderer.h"

namespace Wiesel {

	DescriptorPool::DescriptorPool(uint32_t max) {
		m_MaxDescriptorCount = max;
		m_Descriptors.reserve(max);
	}

	DescriptorPool::~DescriptorPool() {
		Renderer::GetRenderer()->DestroyDescriptors(*this);
	}
}