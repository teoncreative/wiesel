
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_descriptor.h"
#include "w_engine.h"

namespace Wiesel {

	DescriptorData::DescriptorData(uint32_t max) {
		m_MaxDescriptorCount = max;
		m_DescriptorSets.reserve(max);
	}

	DescriptorData::~DescriptorData() {
		Engine::GetRenderer()->DestroyDescriptors(*this);
	}
}