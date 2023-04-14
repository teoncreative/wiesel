
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

#include "w_pch.h"
#include "rendering/w_renderpass.h"
#include "util/w_utils.h"

namespace Wiesel {
	enum CullMode {
		CullModeNone,
		CullModeBack,
		CullModeFront,
		CullModeBoth
	};

	struct PipelineProperties {
		CullMode m_CullFace;
		bool m_EnableWireframe;

		std::vector<char> m_VertexCode;
		std::vector<char> m_FragmentCode;
		Reference<RenderPass> m_RenderPass;

		std::string m_VertexMain = "main";
		std::string m_FragmentMain = "main";
	};

	struct GraphicsPipeline {
		explicit GraphicsPipeline(PipelineProperties properties);
		~GraphicsPipeline();

		PipelineProperties m_Properties;
		VkPipelineLayout m_Layout{};
		VkPipeline m_Pipeline{};
		bool m_IsAllocated;
	};
}