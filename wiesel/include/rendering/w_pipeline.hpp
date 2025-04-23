
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
  bool m_EnableDepthTest = true;
  bool m_EnableDepthWrite = true;
};

struct PushConstant {
  VkShaderStageFlags Flags;
  uint32_t Size;
  uint32_t Offset;
  Ref<void> Ref;
};

struct Pipeline {
  explicit Pipeline(PipelineProperties properties);
  ~Pipeline();

  void SetRenderPass(Ref<RenderPass> pass);
  void AddDescriptorLayout(Ref<DescriptorLayout> layout);
  void AddDynamicState(VkDynamicState state);
  void AddShader(Ref<Shader> shader);
  void SetVertexData(VkVertexInputBindingDescription inputBindingDescription, std::vector<VkVertexInputAttributeDescription> attributeDescriptions);

  template<typename T>
  void AddPushConstant(Ref<T> ref, VkShaderStageFlags flags) {
    m_PushConstants.push_back(PushConstant{
        .Flags = flags,
        .Size = sizeof(T),
        .Offset = static_cast<uint32_t>(m_PushConstants.size()),
        .Ref = std::static_pointer_cast<void>(ref)
    });
  }

  void Bake();

  void Bind(PipelineBindPoint bindPoint);

  PipelineProperties m_Properties;
  std::vector<Ref<Shader>> m_Shaders;
  std::vector<VkDynamicState> m_DynamicStates;
  Ref<RenderPass> m_RenderPass;
  std::vector<Ref<DescriptorLayout>> m_DescriptorLayouts;
  VkPipelineLayout m_Layout{};
  VkPipeline m_Pipeline{};
  bool m_HasVertexBinding = false;
  VkVertexInputBindingDescription m_VertexInputBindingDescription;
  std::vector<VkVertexInputAttributeDescription> m_VertexAttributeDescriptions;
  std::vector<PushConstant> m_PushConstants;
  bool m_IsAllocated = false;
};

}  // namespace Wiesel