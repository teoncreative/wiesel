
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
    : properties_(properties) {
}

Pipeline::~Pipeline() {
  vkDestroyPipeline(Engine::GetRenderer()->GetLogicalDevice(), pipeline_, nullptr);
  vkDestroyPipelineLayout(Engine::GetRenderer()->GetLogicalDevice(), layout_, nullptr);
  is_allocated_ = false;
}

void Pipeline::SetRenderPass(Ref<RenderPass> pass) {
  m_RenderPass = pass;
}

void Pipeline::AddInputLayout(Ref<DescriptorSetLayout> layout) {
  descriptor_layouts_.push_back(layout);
}

void Pipeline::AddDynamicState(VkDynamicState state) {
  dynamic_states_.push_back(state);
}

void Pipeline::AddShader(Ref<Shader> shader) {
  shaders_.push_back({
      .shader = shader
  });
}

void Pipeline::SetVertexData(VkVertexInputBindingDescription input_binding_description, std::vector<VkVertexInputAttributeDescription> attribute_descriptions) {
  vertex_input_binding_descriptions_ = {input_binding_description};
  vertex_attribute_descriptions_ = attribute_descriptions;
  has_vertex_binding_ = true;
}

void Pipeline::SetVertexData(std::vector<VkVertexInputBindingDescription> i, std::vector<VkVertexInputAttributeDescription> attribute_descriptions) {
   vertex_input_binding_descriptions_ = i;
   vertex_attribute_descriptions_ = attribute_descriptions;
   has_vertex_binding_ = true;
}

void Pipeline::Bake() {
  if (is_allocated_) {
    vkDestroyPipeline(Engine::GetRenderer()->GetLogicalDevice(), pipeline_, nullptr);
    vkDestroyPipelineLayout(Engine::GetRenderer()->GetLogicalDevice(), layout_, nullptr);
    is_allocated_ = false;
  }

  std::vector<VkDescriptorSetLayout> layouts;
  layouts.reserve(descriptor_layouts_.size());
  for (const auto& item : descriptor_layouts_) {
    layouts.push_back(item->layout_);
  }

  std::vector<VkPushConstantRange> pushConstants;
  pushConstants.reserve(push_constants_.size());
  for (const auto& item : push_constants_) {
    pushConstants.push_back({
        .stageFlags = item.flags,
        .offset = item.offset,
        .size = item.size
    });
  }
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = layouts.size();
  pipelineLayoutInfo.pSetLayouts = layouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = pushConstants.size();
  pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

  WIESEL_CHECK_VKRESULT(vkCreatePipelineLayout(
      Engine::GetRenderer()->GetLogicalDevice(), &pipelineLayoutInfo, nullptr, &layout_));

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  std::vector<VkSpecializationInfo> specializationInfos;
  specializationInfos.reserve(shaders_.size());
  uint32_t specializationIndex = 0;
  for (const auto& info : shaders_) {
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = GetShaderFlagBitsByType(info.shader->properties_.type);
    stageInfo.module = info.shader->shader_module_;
    stageInfo.pName = info.shader->properties_.main.c_str();
    if (info.specialization.data != nullptr) {
      specializationInfos[specializationIndex] = VkSpecializationInfo{
          .mapEntryCount = static_cast<uint32_t>(info.specialization.map_entries.size()),
          .pMapEntries = info.specialization.map_entries.data(),
          .dataSize = sizeof(info.specialization.data_size),
          .pData = info.specialization.data
      };
      stageInfo.pSpecializationInfo = &specializationInfos[specializationIndex];
      specializationIndex++;
    }
    shaderStages.push_back(stageInfo);
  }

  std::vector<VkDynamicState> dynamicStates;
  dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
  for (const auto& item : dynamic_states_) {
    dynamicStates.push_back(item);
  }

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  if (has_vertex_binding_) {
    vertexInputInfo.vertexBindingDescriptionCount =
        static_cast<uint32_t>(vertex_input_binding_descriptions_.size());
    vertexInputInfo.pVertexBindingDescriptions = vertex_input_binding_descriptions_.data();
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(vertex_attribute_descriptions_.size());
    vertexInputInfo.pVertexAttributeDescriptions = vertex_attribute_descriptions_.data();
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
  if (properties_.enable_wireframe) {
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
  } else {
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  }
  rasterizer.lineWidth = 1.0f;
  switch (properties_.cull_mode) {
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
  multisampling.rasterizationSamples = properties_.msaa_samples;

  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
  for (const auto& item : m_RenderPass->attachments_) {
    if (item.type != AttachmentTextureType::Color && item.type != AttachmentTextureType::Offscreen) {
      continue;
    }
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (properties_.enable_alpha_blending) {
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
    colorBlendAttachments.push_back(colorBlendAttachment);
  }
  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = colorBlendAttachments.size();
  colorBlending.pAttachments = colorBlendAttachments.data();

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  if (properties_.enable_depth_test) {
    depthStencil.depthTestEnable = VK_TRUE;
  } else {
    depthStencil.depthTestEnable = VK_FALSE;
  }
  if (properties_.enable_depth_write) {
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
  pipelineInfo.layout = layout_;
  pipelineInfo.renderPass = m_RenderPass->GetVulkanHandle();
  pipelineInfo.subpass = 0;

  WIESEL_CHECK_VKRESULT(
      vkCreateGraphicsPipelines(Engine::GetRenderer()->GetLogicalDevice(), VK_NULL_HANDLE, 1,
                                &pipelineInfo, nullptr, &pipeline_));

  is_allocated_ = true;
}

void Pipeline::Bind(PipelineBindPoint bind_point) {
  vkCmdBindPipeline(Engine::GetRenderer()->GetCommandBuffer().handle_, ToVkPipelineBindPoint(bind_point),
                    pipeline_);
  for (const auto& item : push_constants_) {
    vkCmdPushConstants(Engine::GetRenderer()->GetCommandBuffer().handle_, layout_,
                       item.flags, 0, item.size, item.ref.get());
  }
}

}  // namespace Wiesel