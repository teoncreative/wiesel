
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_renderpass.hpp"

#include "w_engine.hpp"
#include "rendering/w_framebuffer.hpp"
#include "rendering/w_texture.hpp"

namespace Wiesel {

VkPipelineBindPoint ToVkPipelineBindPoint(PipelineBindPoint point) {
  switch(point) {
    case PipelineBindPointGraphics:
      return VK_PIPELINE_BIND_POINT_GRAPHICS;
    case PipelineBindPointCompute:
      return VK_PIPELINE_BIND_POINT_COMPUTE;
    case PipelineBindPointRayTracingKHR:
      return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
    case PipelineBindPointSubpassShadingHuawei:
      return VK_PIPELINE_BIND_POINT_SUBPASS_SHADING_HUAWEI;
    default:
      return VK_PIPELINE_BIND_POINT_MAX_ENUM;
  }
}

RenderPass::RenderPass(PassType passType) : m_PassType(passType) {

}

RenderPass::~RenderPass() {
  vkDestroyRenderPass(Engine::GetRenderer()->m_LogicalDevice, m_RenderPass, nullptr);
  //Engine::GetRenderer()->DestroyRenderPass(*this);
}

void RenderPass::Attach(Ref<AttachmentTexture> attachment) {
  m_Attachments.push_back({
      .Type = attachment->m_Type,
      .Format = attachment->m_Format,
      .MsaaSamples = attachment->m_MsaaSamples
  });
}

void RenderPass::Attach(AttachmentTextureInfo&& info) {
  m_Attachments.push_back(info);
}
void RenderPass::Bake() {
  std::vector<VkAttachmentDescription> descriptions;
  std::vector<VkAttachmentReference> colorAttachmentRefs;
  std::vector<VkAttachmentReference> resolveAttachmentRefs;
  std::vector<VkAttachmentReference> depthAttachmentRefs; // can only be one

  uint32_t index = 0;
  for (const auto& item : m_Attachments) {
    if (item.Type == AttachmentTextureType::DepthStencil && depthAttachmentRefs.empty()) {
      descriptions.push_back({
          .format = item.Format,
          .samples = item.MsaaSamples,
          .loadOp = m_PassType == PassType::Lighting ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = m_PassType == PassType::Geometry || m_PassType == PassType::Shadow ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
      });
      depthAttachmentRefs.push_back({
          .attachment = index,
          .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
      });
      index++;
    } else if (item.Type == AttachmentTextureType::Color || item.Type == AttachmentTextureType::Offscreen) {
      descriptions.push_back({
          .format = item.Format,
          .samples = item.MsaaSamples,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      });
      colorAttachmentRefs.push_back({
          .attachment = index,
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
      });
      index++;
    } else if (item.Type == AttachmentTextureType::Resolve || item.Type == AttachmentTextureType::SwapChain) {
      descriptions.push_back({
          .format = item.Format,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout = m_PassType == PassType::Present ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      });
      resolveAttachmentRefs.push_back({
          .attachment = index,
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
      });
      index++;
    }
  }

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = colorAttachmentRefs.size();
  subpass.pColorAttachments = colorAttachmentRefs.data();
  if (!resolveAttachmentRefs.empty()) {
    subpass.pResolveAttachments = resolveAttachmentRefs.data();
  } else {
    subpass.pResolveAttachments = nullptr;
  }
  if (!depthAttachmentRefs.empty()) {
    subpass.pDepthStencilAttachment = depthAttachmentRefs.data();
  } else {
    subpass.pDepthStencilAttachment = nullptr;
  }

  std::vector<VkSubpassDependency> dependencies{};
  /*if (m_PassType == PassType::Geometry || m_PassType == PassType::PostProcess) {
    dependencies.push_back({
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_NONE_KHR,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    });
    dependencies.push_back({
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    });
  } else {
    dependencies.push_back({
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    });
  }*/
  if (m_PassType == PassType::Shadow) {
    dependencies.push_back({
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
      .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    });
    dependencies.push_back({
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    });
  } else {
    dependencies.push_back({
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    });
  }
  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(descriptions.size());
  renderPassInfo.pAttachments = descriptions.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  if (vkCreateRenderPass(Engine::GetRenderer()->m_LogicalDevice, &renderPassInfo, nullptr,
                         &m_RenderPass) != VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

void RenderPass::Begin(Ref<Framebuffer> framebuffer, const Colorf& clearColor) {
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_RenderPass;
  renderPassInfo.framebuffer = framebuffer->m_Handle;
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent.width = framebuffer->m_Extent.x;
  renderPassInfo.renderArea.extent.height = framebuffer->m_Extent.y;

  std::vector<VkClearValue> clearValues{};
  if (m_PassType == PassType::Shadow) {
    clearValues.resize(1);
    clearValues[0].depthStencil = {1.0f, 0};
  } else {
    clearValues.resize(2);
    clearValues[0].color = {{clearColor.Red, clearColor.Green,
                             clearColor.Blue, clearColor.Alpha}};
    clearValues[1].depthStencil = {1.0f, 0};
  }
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();
  vkCmdBeginRenderPass(Engine::GetRenderer()->GetCommandBuffer().m_Handle, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPass::End() {
  vkCmdEndRenderPass(Engine::GetRenderer()->GetCommandBuffer().m_Handle);
}

Ref<Framebuffer> RenderPass::CreateFramebuffer(uint32_t index, std::span<AttachmentTexture*> attachments, glm::vec2 extent) {
  bool hasDepth = false;
  std::vector<VkImageView> views;
  for (const auto& item : attachments) {
    if (item->m_Type == AttachmentTextureType::DepthStencil && !hasDepth) {
      views.push_back(item->m_ImageViews[index]->m_Handle);
      hasDepth = true;
    } else if (item->m_Type == AttachmentTextureType::Color || item->m_Type == AttachmentTextureType::Offscreen || item->m_Type == AttachmentTextureType::SwapChain ||
               item->m_Type == AttachmentTextureType::Resolve) {
      views.push_back(item->m_ImageViews[index]->m_Handle);
    }
  }
  // TODO check if views match the expected framebuffer attachment size
  return CreateReference<Framebuffer>(views, extent, *this);
}

Ref<Framebuffer> RenderPass::CreateFramebuffer(uint32_t index, std::span<ImageView*> imageViews, glm::vec2 extent) {
  std::vector<VkImageView> views;
  for (const auto& item : imageViews) {
    views.push_back(item->m_Handle);
  }
  // TODO check if views match the expected framebuffer attachment size
  return CreateReference<Framebuffer>(views, extent, *this);
}

}  // namespace Wiesel