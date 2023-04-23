
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_pch.hpp"
#include "util/w_utils.hpp"

namespace Wiesel {
	class DescriptorLayout {
	public:
		explicit DescriptorLayout();
		~DescriptorLayout();

		bool m_Allocated;
		VkDescriptorSetLayout m_Layout;
	};
}