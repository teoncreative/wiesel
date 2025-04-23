
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
  RenderPass(PassType passType);
  ~RenderPass();

  void Attach(Ref<AttachmentTexture> ref);
  void Attach(AttachmentTextureInfo&& info);

  void Bake();

  void Begin(Ref<Framebuffer> framebuffer, const Colorf& clearColor);
  void End();

  Ref<Framebuffer> CreateFramebuffer(uint32_t index, std::span<AttachmentTexture*> attachments, glm::vec2 extent);
  Ref<Framebuffer> CreateFramebuffer(uint32_t index, std::span<ImageView*> views, glm::vec2 extent);

  const VkRenderPass& GetVulkanHandle() const { return m_RenderPass; }
 private:
  PassType m_PassType;
  VkRenderPass m_RenderPass;
  std::vector<AttachmentTextureInfo> m_Attachments;

};


}  // namespace Wiesel