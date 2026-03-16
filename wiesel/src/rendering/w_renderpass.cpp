
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

RenderPass::RenderPass(PassType pass_type, const std::string& debug_name) : pass_type_(pass_type), debug_name(debug_name) {

}

RenderPass::~RenderPass() {
  vkDestroyRenderPass(Engine::renderer()->logical_device_, render_pass_, nullptr);
  //Engine::renderer()->DestroyRenderPass(*this);
}

void RenderPass::AttachOutput(Ref<AttachmentTexture> attachment) {
  attachments_.push_back({
      .type = attachment->type_,
      .format = attachment->format_,
      .msaa_mode = attachment->sampling_mode_
  });
}

void RenderPass::AttachOutput(AttachmentTextureInfo&& info) {
  attachments_.push_back(info);
}

void RenderPass::Bake() {
  std::vector<VkAttachmentDescription> descriptions;
  std::vector<VkAttachmentReference> colorAttachmentRefs;
  std::vector<VkAttachmentReference> resolveAttachmentRefs;
  std::vector<VkAttachmentReference> depthAttachmentRefs; // can only be one

  uint32_t index = 0;
  for (const auto& item : attachments_) {
    if (item.type == AttachmentTextureType::DepthStencil && depthAttachmentRefs.empty()) {
      bool loads_depth = pass_type_ == PassType::Lighting || pass_type_ == PassType::ForwardTransparency;
      descriptions.push_back({
          .format = item.format,
          .samples = ToVkSampleCountFlagBits(item.msaa_mode),
          .loadOp = loads_depth ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = pass_type_ == PassType::Geometry || pass_type_ == PassType::Shadow ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = loads_depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
      });
      depthAttachmentRefs.push_back({
          .attachment = index,
          .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
      });
      index++;
    } else if (item.type == AttachmentTextureType::Color || item.type == AttachmentTextureType::Offscreen) {
      descriptions.push_back({
          .format = item.format,
          .samples = ToVkSampleCountFlagBits(item.msaa_mode),
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
    } else if (item.type == AttachmentTextureType::Resolve || item.type == AttachmentTextureType::SwapChain) {
      bool used_as_resolve = item.msaa_mode > SamplingMode::DISABLED || item.type == AttachmentTextureType::Resolve;
      if (used_as_resolve) {
        descriptions.push_back({
            .format = item.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = pass_type_ == PassType::Present ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        });
        resolveAttachmentRefs.push_back({
            .attachment = index,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        });
      } else {
        // SwapChain used as direct color target (no MSAA resolve needed)
        descriptions.push_back({
            .format = item.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        });
        colorAttachmentRefs.push_back({
            .attachment = index,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        });
      }
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
  if (pass_type_ == PassType::Shadow) {
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

  if (vkCreateRenderPass(Engine::renderer()->logical_device_, &renderPassInfo, nullptr,
                         &render_pass_) != VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }

  Engine::renderer()->SetObjectName(VK_OBJECT_TYPE_RENDER_PASS,
                                       reinterpret_cast<uint64_t>(render_pass_),
                                       debug_name.c_str());
}

void RenderPass::Begin(Ref<Framebuffer> framebuffer, const Colorf& clear_color) {
  VkRenderPassBeginInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.renderPass = render_pass_;
  render_pass_info.framebuffer = framebuffer->handle_;
  render_pass_info.renderArea.offset = {0, 0};
  render_pass_info.renderArea.extent.width = framebuffer->extent_.x;
  render_pass_info.renderArea.extent.height = framebuffer->extent_.y;

  std::vector<VkClearValue> clearValues{};
  for (const auto& item : attachments_) {
    if (item.type == AttachmentTextureType::DepthStencil) {
      clearValues.push_back({
          .depthStencil = {1.0f, 0}
      });
    } else {
      clearValues.push_back({
          .color = {clear_color.red, clear_color.green,
                    clear_color.blue, clear_color.alpha}
      });
    }
  }
  render_pass_info.clearValueCount = static_cast<uint32_t>(clearValues.size());
  render_pass_info.pClearValues = clearValues.data();
  vkCmdBeginRenderPass(Engine::renderer()->GetCommandBuffer().handle_, &render_pass_info,
                       VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPass::End() {
  vkCmdEndRenderPass(Engine::renderer()->GetCommandBuffer().handle_);
}

// Change these to take span of Ref<ImageView> instead.
Ref<Framebuffer> RenderPass::CreateFramebuffer(uint32_t index, std::span<AttachmentTexture* const> output_attachments, glm::vec2 extent) {
  bool has_depth = false;
  std::vector<VkImageView> views;
  for (const auto& item : output_attachments) {
    if (item->type_ == AttachmentTextureType::DepthStencil && !has_depth) {
      views.push_back(item->image_views_[index]->handle_);
      has_depth = true;
    } else if (item->type_ == AttachmentTextureType::Color || item->type_ == AttachmentTextureType::Offscreen || item->type_ == AttachmentTextureType::SwapChain ||
               item->type_ == AttachmentTextureType::Resolve) {
      views.push_back(item->image_views_[index]->handle_);
    }
  }
  // TODO check if views match the expected framebuffer attachment size
  return CreateReference<Framebuffer>(views, extent, *this);
}

Ref<Framebuffer> RenderPass::CreateFramebuffer(std::span<ImageView*> output_views, glm::vec2 extent) {
  std::vector<VkImageView> views;
  for (const auto& item : output_views) {
    views.push_back(item->handle_);
  }
  // TODO check if views match the expected framebuffer attachment size
  return CreateReference<Framebuffer>(views, extent, *this);
}

Ref<Framebuffer> RenderPass::CreateFramebuffer(std::initializer_list<Ref<ImageView>> output_views, glm::vec2 extent) {
  std::vector<VkImageView> views;
  for (const auto& item : output_views) {
    views.push_back(item->handle_);
  }
  // TODO check if views match the expected framebuffer attachment size
  return CreateReference<Framebuffer>(views, extent, *this);
}
}  // namespace Wiesel