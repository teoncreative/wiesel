
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_pipeline.h"

#include <algorithm>

#include "rendering/w_descriptor.h"

#include "w_engine.h"

namespace wiesel {

static VkCompareOp ToVkCompareOp(CompareOp op) {
  switch (op) {
    case CompareOpLess:
      return VK_COMPARE_OP_LESS;
    case CompareOpLessOrEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOpGreater:
      return VK_COMPARE_OP_GREATER;
    case CompareOpGreaterOrEqual:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareOpEqual:
      return VK_COMPARE_OP_EQUAL;
    case CompareOpAlways:
      return VK_COMPARE_OP_ALWAYS;
    case CompareOpNever:
      return VK_COMPARE_OP_NEVER;
    default:
      return VK_COMPARE_OP_LESS;
  }
}

Pipeline::Pipeline(PipelineProperties properties) : properties_(properties) {}

Pipeline::~Pipeline() {
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }
  VkPipeline pipeline = pipeline_;
  VkPipelineLayout layout = layout_;
  VkDevice device = renderer->GetLogicalDevice();
  renderer->GetDeletionQueue().Push([device, pipeline, layout]() {
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
  });
  is_allocated_ = false;
}

void Pipeline::SetRenderPass(std::shared_ptr<RenderPass> pass) {
  render_pass_ = pass;
}

void Pipeline::AddInputLayout(std::shared_ptr<DescriptorSetLayout> layout) {
  descriptor_layouts_.push_back(layout);
}

void Pipeline::AddDynamicState(VkDynamicState state) {
  dynamic_states_.push_back(state);
}

void Pipeline::AddShader(std::shared_ptr<Shader> shader) {
  shaders_.push_back({.shader = shader});
}

void Pipeline::SetVertexData(
    VkVertexInputBindingDescription input_binding_description,
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions) {
  vertex_input_binding_descriptions_ = {input_binding_description};
  vertex_attribute_descriptions_ = attribute_descriptions;
  has_vertex_binding_ = true;
}

void Pipeline::SetVertexData(
    std::vector<VkVertexInputBindingDescription> i,
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions) {
  vertex_input_binding_descriptions_ = i;
  vertex_attribute_descriptions_ = attribute_descriptions;
  has_vertex_binding_ = true;
}

void Pipeline::Bake() {
  LOG_DEBUG("Creating pipeline with {} samples",
            (uint64_t)properties_.sampling_mode);
  if (is_allocated_) {
    vkDestroyPipeline(Engine::renderer()->GetLogicalDevice(), pipeline_,
                      nullptr);
    vkDestroyPipelineLayout(Engine::renderer()->GetLogicalDevice(), layout_,
                            nullptr);
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
    pushConstants.push_back(
        {.stageFlags = item.flags, .offset = item.offset, .size = item.size});
  }
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = layouts.size();
  pipelineLayoutInfo.pSetLayouts = layouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = pushConstants.size();
  pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

  WIESEL_CHECK_VKRESULT(
      vkCreatePipelineLayout(Engine::renderer()->GetLogicalDevice(),
                             &pipelineLayoutInfo, nullptr, &layout_));

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  std::vector<VkSpecializationInfo> specializationInfos;
  specializationInfos.reserve(shaders_.size());
  uint32_t specializationIndex = 0;
  for (const auto& info : shaders_) {
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = GetShaderFlagBitsByType(info.shader->properties_.type);
    stageInfo.module = info.shader->shader_module_;
    stageInfo.pName = info.shader->properties_.main.c_str();
    if (info.specialization.data != nullptr) {
      specializationInfos[specializationIndex] = VkSpecializationInfo{
          .mapEntryCount =
              static_cast<uint32_t>(info.specialization.map_entries.size()),
          .pMapEntries = info.specialization.map_entries.data(),
          .dataSize = sizeof(info.specialization.data_size),
          .pData = info.specialization.data};
      stageInfo.pSpecializationInfo = &specializationInfos[specializationIndex];
      specializationIndex++;
    }
    shaderStages.push_back(stageInfo);
  }

  std::vector<VkDynamicState> dynamicStates;
  dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
  if (properties_.enable_stencil_test) {
    dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
    dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
  }
  for (const auto& item : dynamic_states_) {
    if (std::find(dynamicStates.begin(), dynamicStates.end(), item) ==
        dynamicStates.end()) {
      dynamicStates.push_back(item);
    }
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
    vertexInputInfo.pVertexBindingDescriptions =
        vertex_input_binding_descriptions_.data();
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(vertex_attribute_descriptions_.size());
    vertexInputInfo.pVertexAttributeDescriptions =
        vertex_attribute_descriptions_.data();
  } else {
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
  }

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  switch (properties_.topology) {
    case PrimitiveTopology::LineList:
      inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
      break;
    default:
      inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      break;
  }
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
  rasterizer.lineWidth = properties_.line_width;
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
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

  bool use_depth_bias = properties_.depth_bias_constant != 0.0f ||
                        properties_.depth_bias_slope != 0.0f;
  rasterizer.depthBiasEnable = use_depth_bias ? VK_TRUE : VK_FALSE;
  rasterizer.depthBiasConstantFactor = properties_.depth_bias_constant;
  rasterizer.depthBiasSlopeFactor = properties_.depth_bias_slope;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples =
      ToVkSampleCountFlagBits(properties_.sampling_mode);

  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
  for (const auto& item : render_pass_->attachments_) {
    if (item.type != AttachmentTextureType::Color &&
        item.type != AttachmentTextureType::Offscreen &&
        item.type != AttachmentTextureType::SwapChain) {
      continue;
    }
    // Skip resolve targets - they don't count as color attachments in the subpass
    if (item.type == AttachmentTextureType::SwapChain ||
        item.type == AttachmentTextureType::Resolve) {
      bool used_as_resolve = item.msaa_mode > SamplingMode::DISABLED ||
                             item.type == AttachmentTextureType::Resolve;
      if (used_as_resolve) {
        continue;
      }
    }
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        properties_.color_write_enabled
            ? (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
            : 0;
    // Integer formats (e.g. R32_UINT for entity IDs) cannot use blending
    bool is_integer_format =
        item.format == VK_FORMAT_R32_UINT || item.format == VK_FORMAT_R32_SINT;
    if (properties_.enable_alpha_blending && !is_integer_format) {
      colorBlendAttachment.blendEnable = VK_TRUE;
      colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
      colorBlendAttachment.dstColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
      colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      colorBlendAttachment.dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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
  depthStencil.depthCompareOp = ToVkCompareOp(properties_.depth_compare_op);
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f;
  depthStencil.maxDepthBounds = 1.0f;
  depthStencil.stencilTestEnable =
      properties_.enable_stencil_test ? VK_TRUE : VK_FALSE;
  if (properties_.enable_stencil_test) {
    VkStencilOpState stencil_op{};
    stencil_op.failOp = VK_STENCIL_OP_KEEP;
    stencil_op.passOp = properties_.stencil_pass_op;
    stencil_op.depthFailOp = VK_STENCIL_OP_KEEP;
    stencil_op.compareOp = properties_.stencil_compare_op;
    stencil_op.compareMask = 0xFF;
    stencil_op.writeMask = 0xFF;
    stencil_op.reference = 0;
    depthStencil.front = stencil_op;
    depthStencil.back = stencil_op;
  }

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
  pipelineInfo.renderPass = render_pass_->GetVulkanHandle();
  pipelineInfo.subpass = 0;

  WIESEL_CHECK_VKRESULT(vkCreateGraphicsPipelines(
      Engine::renderer()->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo,
      nullptr, &pipeline_));

  is_allocated_ = true;
}

void Pipeline::Bind(PipelineBindPoint bind_point, VkCommandBuffer cmd) {
  auto renderer = Engine::renderer();
  VkCommandBuffer command_buffer = renderer->ResolveCmd(cmd);
  vkCmdBindPipeline(command_buffer, ToVkPipelineBindPoint(bind_point),
                    pipeline_);
  PushConstants(command_buffer);
  renderer->SetBoundPipeline(this);
}

void Pipeline::PushConstants(VkCommandBuffer cmd) {
  VkCommandBuffer command_buffer = Engine::renderer()->ResolveCmd(cmd);
  for (const auto& item : push_constants_) {
    vkCmdPushConstants(command_buffer, layout_, item.flags, 0, item.size,
                       item.ptr.get());
  }
}

void Pipeline::BindDescriptorSets(
    VkCommandBuffer cmd,
    const std::vector<std::shared_ptr<DescriptorSet>>& sets) {
  VkCommandBuffer command_buffer = Engine::renderer()->ResolveCmd(cmd);
  std::vector<VkDescriptorSet> vk_sets;
  vk_sets.reserve(sets.size());
  for (const auto& s : sets) {
    vk_sets.push_back(s->descriptor_set_);
  }
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          layout_, 0, static_cast<uint32_t>(vk_sets.size()),
                          vk_sets.data(), 0, nullptr);
}

}  // namespace wiesel