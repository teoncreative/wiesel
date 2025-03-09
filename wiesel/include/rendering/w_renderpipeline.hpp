
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

#include "rendering/w_renderpass.hpp"
#include "util/w_utils.hpp"
#include "w_descriptorlayout.hpp"
#include "w_pch.hpp"
#include "w_shader.hpp"

namespace Wiesel {
enum CullMode { CullModeNone, CullModeBack, CullModeFront, CullModeBoth };

struct PipelineProperties {
  CullMode m_CullFace;
  bool m_EnableWireframe;
  Ref<RenderPass> m_RenderPass;
  Ref<DescriptorLayout> m_DescriptorLayout;
  Ref<Shader> m_VertexShader;
  Ref<Shader> m_FragmentShader;
  bool m_EnableAlphaBlending;
  uint32_t m_ViewportWidth;
  uint32_t m_ViewportHeight;
};

struct GraphicsPipeline {
  explicit GraphicsPipeline(PipelineProperties properties);
  ~GraphicsPipeline();

  PipelineProperties m_Properties;
  VkPipelineLayout m_Layout{};
  VkPipeline m_Pipeline{};
  bool m_IsAllocated;
};

}  // namespace Wiesel