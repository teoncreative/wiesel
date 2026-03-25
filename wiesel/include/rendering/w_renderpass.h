
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "util/w_color.h"
#include "util/w_utils.h"
#include "w_pch.h"
#include "w_shader.h"
#include "w_texture.h"

namespace Wiesel {
// I hate forward declarations but in this case it's required
class Framebuffer;
class AttachmentTexture;

enum class PassType {
  Geometry,
  PostProcess,
  Lighting,
  ForwardTransparency,
  Shadow,
  Present
};

enum PipelineBindPoint {
  PipelineBindPointGraphics,
  PipelineBindPointCompute,
#ifdef VK_ENABLE_BETA_EXTENSIONS
  PipelineBindPointExecGraphAMDX,
#endif
  PipelineBindPointRayTracingKHR,
  PipelineBindPointSubpassShadingHuawei
};

VkPipelineBindPoint ToVkPipelineBindPoint(PipelineBindPoint point);

class RenderPass {
 public:
  RenderPass(PassType pass_type, const std::string& debug_name);
  ~RenderPass();

  void AttachOutput(std::shared_ptr<AttachmentTexture> ref);
  void AttachOutput(AttachmentTextureInfo&& info);

  void Bake();

  void Begin(std::shared_ptr<Framebuffer> framebuffer,
             const Colorf& clear_color, VkCommandBuffer cmd = VK_NULL_HANDLE);
  void End(VkCommandBuffer cmd = VK_NULL_HANDLE);

  std::shared_ptr<Framebuffer> CreateFramebuffer(
      uint32_t index, std::span<AttachmentTexture* const> output_attachments,
      glm::vec2 extent);
  std::shared_ptr<Framebuffer> CreateFramebuffer(
      std::span<ImageView*> output_views, glm::vec2 extent);
  std::shared_ptr<Framebuffer> CreateFramebuffer(
      std::initializer_list<std::shared_ptr<ImageView>> output_views,
      glm::vec2 extent);

  std::shared_ptr<Framebuffer> CreateFramebuffer(
      uint32_t index,
      std::initializer_list<AttachmentTexture* const> output_attachments,
      glm::vec2 extent) {
    return CreateFramebuffer(
        index, std::span(output_attachments.begin(), output_attachments.end()),
        extent);
  }

  const VkRenderPass& GetVulkanHandle() const { return render_pass_; }

 private:
  friend class Pipeline;
  PassType pass_type_;
  std::string debug_name;
  VkRenderPass render_pass_;
  std::vector<AttachmentTextureInfo> attachments_;
};

}  // namespace Wiesel