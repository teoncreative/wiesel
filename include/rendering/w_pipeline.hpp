
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
#include "rendering/w_renderpass.hpp"
#include "util/w_utils.hpp"
#include "w_shader.hpp"

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

		Reference<RenderPass> m_RenderPass;

		Reference<Shader> m_VertexShader;
		Reference<Shader> m_FragmentShader;
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