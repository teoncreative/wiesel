
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
  VkSampleCountFlagBits m_MsaaSamples;
  CullMode m_CullMode;
  bool m_EnableWireframe;
  bool m_EnableAlphaBlending;
};

struct GraphicsPipeline {
  explicit GraphicsPipeline(PipelineProperties properties);
  ~GraphicsPipeline();

  void SetRenderPass(Ref<RenderPass> pass);
  void SetDescriptorLayout(Ref<DescriptorLayout> layout);
  void AddDynamicState(VkDynamicState state);
  void AddShader(Ref<Shader> shader);
  void SetVertexData(VkVertexInputBindingDescription inputBindingDescription, std::vector<VkVertexInputAttributeDescription> attributeDescriptions);
  void Bake();

  void Bind(PipelineBindPoint bindPoint);

  PipelineProperties m_Properties;
  std::vector<Ref<Shader>> m_Shaders;
  std::vector<VkDynamicState> m_DynamicStates;
  Ref<RenderPass> m_RenderPass;
  Ref<DescriptorLayout> m_DescriptorLayout;
  VkPipelineLayout m_Layout{};
  VkPipeline m_Pipeline{};
  VkVertexInputBindingDescription m_VertexInputBindingDescription;
  std::vector<VkVertexInputAttributeDescription> m_VertexAttributeDescriptions;

  bool m_IsAllocated = false;
};

}  // namespace Wiesel