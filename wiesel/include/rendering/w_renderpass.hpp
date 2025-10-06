
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
#include "util/w_color.hpp"
#include "util/w_utils.hpp"
#include "w_shader.hpp"
#include "w_texture.hpp"

namespace Wiesel {
// I hate forward declarations but in this case it's required
class Framebuffer;
class AttachmentTexture;

enum class PassType {
  Geometry,
  PostProcess,
  Lighting,
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
  RenderPass(PassType pass_type);
  ~RenderPass();

  void AttachOutput(Ref<AttachmentTexture> ref);
  void AttachOutput(AttachmentTextureInfo&& info);

  void Bake();

  void Begin(Ref<Framebuffer> framebuffer, const Colorf& clear_color);
  void End();

  Ref<Framebuffer> CreateFramebuffer(uint32_t index, std::span<AttachmentTexture*> output_attachments, glm::vec2 extent);
  Ref<Framebuffer> CreateFramebuffer(uint32_t index, std::span<ImageView*> output_views, glm::vec2 extent);
  Ref<Framebuffer> CreateFramebuffer(uint32_t index, std::initializer_list<Ref<ImageView>> output_views, glm::vec2 extent);

  const VkRenderPass& GetVulkanHandle() const { return render_pass_; }
 private:
  friend class Pipeline;
  PassType pass_type_;
  VkRenderPass render_pass_;
  std::vector<AttachmentTextureInfo> attachments_;

};


}  // namespace Wiesel