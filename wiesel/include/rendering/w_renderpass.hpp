
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

namespace Wiesel {
struct RenderPassProperties {
  VkFormat m_SwapChainImageFormat;
  VkSampleCountFlagBits m_MsaaSamples;
  VkFormat m_DepthFormat;
};

struct RenderPass {
  explicit RenderPass(RenderPassProperties properties);
  ~RenderPass();

  RenderPassProperties m_Properties;
  VkRenderPass m_Pass{};
};
}  // namespace Wiesel