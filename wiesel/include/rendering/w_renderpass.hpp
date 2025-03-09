
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

#include "w_shader.hpp"
#include "util/w_utils.hpp"
#include "util/w_color.hpp"

namespace Wiesel {
enum class PassType {
  Geometry,
  PostProcess
};

struct RenderPassSpecification {
  PassType m_PassType;
  VkFormat m_DepthFormat;
  VkFormat m_SwapChainImageFormat;
  VkSampleCountFlagBits m_MsaaSamples;

};

class RenderPass {
 public:
  RenderPass(RenderPassSpecification specification);
  ~RenderPass();

  void Bake();
  bool Validate();

  void Bind();

  const VkRenderPass& GetVulkanHandle() const { return m_RenderPass; }
 private:
  RenderPassSpecification m_Specification;
  VkRenderPass m_RenderPass;

};


}  // namespace Wiesel