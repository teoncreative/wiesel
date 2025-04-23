
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_pipeline.hpp"

#include "w_engine.hpp"

namespace Wiesel {

Pipeline::Pipeline(PipelineProperties properties)
    : m_Properties(properties) {
}

Pipeline::~Pipeline() {
  vkDestroyPipeline(Engine::GetRenderer()->GetLogicalDevice(), m_Pipeline, nullptr);
  vkDestroyPipelineLayout(Engine::GetRenderer()->GetLogicalDevice(), m_Layout, nullptr);
  m_IsAllocated = false;
}

void Pipeline::SetRenderPass(Ref<RenderPass> pass) {
  m_RenderPass = pass;
}

void Pipeline::AddDescriptorLayout(Ref<DescriptorLayout> layout) {
  m_DescriptorLayouts.push_back(layout);
}

void Pipeline::AddDynamicState(VkDynamicState state) {
  m_DynamicStates.push_back(state);
}

void Pipeline::AddShader(Ref<Shader> shader) {
  m_Shaders.push_back(shader);
}

void Pipeline::SetVertexData(VkVertexInputBindingDescription inputBindingDescription, std::vector<VkVertexInputAttributeDescription> attributeDescriptions) {
   m_VertexInputBindingDescription = inputBindingDescription;
   m_VertexAttributeDescriptions = attributeDescriptions;
   m_HasVertexBinding = true;
}

void Pipeline::Bake() {
  if (m_IsAllocated) {
    vkDestroyPipeline(Engine::GetRenderer()->GetLogicalDevice(), m_Pipeline, nullptr);
    vkDestroyPipelineLayout(Engine::GetRenderer()->GetLogicalDevice(), m_Layout, nullptr);
    m_IsAllocated = false;
  }
  std::vector<VkDescriptorSetLayout> layouts;
  for (const auto& item : m_DescriptorLayouts) {
    layouts.push_back(item->m_Layout);
  }

  std::vector<VkPushConstantRange> pushConstants;
  for (const auto& item : m_PushConstants) {
    pushConstants.push_back({
        .stageFlags = item.Flags,
        .offset = item.Offset,
        .size = item.Size
    });
  }
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = layouts.size();
  pipelineLayoutInfo.pSetLayouts = layouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = pushConstants.size();
  pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

  WIESEL_CHECK_VKRESULT(vkCreatePipelineLayout(
      Engine::GetRenderer()->GetLogicalDevice(), &pipelineLayoutInfo, nullptr, &m_Layout));

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  for (const auto& shader : m_Shaders) {
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = GetShaderFlagBitsByType(shader->m_Properties.Type);
    stageInfo.module = shader->m_ShaderModule;
    stageInfo.pName = shader->m_Properties.Main.c_str();
    shaderStages.push_back(stageInfo);
  }

  std::vector<VkDynamicState> dynamicStates;
  dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
  for (const auto& item : m_DynamicStates) {
    dynamicStates.push_back(item);
  }

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  if (m_HasVertexBinding) {
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &m_VertexInputBindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(m_VertexAttributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = m_VertexAttributeDescriptions.data();
  } else {
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
  }

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = nullptr;
  viewportState.scissorCount = 1;
  viewportState.pScissors = nullptr;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  /*
     * VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
     * VK_POLYGON_MODE_LINE: polygon edges are drawn as lines
     * VK_POLYGON_MODE_POINT: polygon vertices are drawn as points
     */
  if (m_Properties.m_EnableWireframe) {
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
  } else {
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  }
  rasterizer.lineWidth = 1.0f;
  switch (m_Properties.m_CullMode) {
    case CullModeNone:
      rasterizer.cullMode = VK_CULL_MODE_NONE;
      break;
    case CullModeFront:
      rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
      break;
    case CullModeBack:
      rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
      break;
    case CullModeBoth:
      rasterizer.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
      break;
  }
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = m_Properties.m_MsaaSamples;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  if (m_Properties.m_EnableAlphaBlending) {
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  } else {
    colorBlendAttachment.blendEnable = VK_FALSE;
  }

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  if (m_Properties.m_EnableDepthTest) {
    depthStencil.depthTestEnable = VK_TRUE;
  } else {
    depthStencil.depthTestEnable = VK_FALSE;
  }
  if (m_Properties.m_EnableDepthWrite) {
    depthStencil.depthWriteEnable = VK_TRUE;
  } else {
    depthStencil.depthWriteEnable = VK_FALSE;
  }
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f;
  depthStencil.maxDepthBounds = 1.0f;

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();

  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = m_Layout;
  pipelineInfo.renderPass = m_RenderPass->GetVulkanHandle();
  pipelineInfo.subpass = 0;

  WIESEL_CHECK_VKRESULT(
      vkCreateGraphicsPipelines(Engine::GetRenderer()->GetLogicalDevice(), VK_NULL_HANDLE, 1,
                                &pipelineInfo, nullptr, &m_Pipeline));

  m_IsAllocated = true;
}

void Pipeline::Bind(PipelineBindPoint bindPoint) {
  vkCmdBindPipeline(Engine::GetRenderer()->GetCommandBuffer().m_Handle, ToVkPipelineBindPoint(bindPoint),
                    m_Pipeline);
  for (const auto& item : m_PushConstants) {
    vkCmdPushConstants(Engine::GetRenderer()->GetCommandBuffer().m_Handle, m_Layout,
                       item.Flags, 0, item.Size, item.Ref.get());
  }
}

}  // namespace Wiesel