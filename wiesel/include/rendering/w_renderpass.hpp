
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

namespace Wiesel {
struct GraphicsRenderPassProps {
  VkFormat m_SwapChainImageFormat;
  VkSampleCountFlagBits m_MsaaSamples;
  VkFormat m_DepthFormat;
};

struct GraphicsRenderPass {
  explicit GraphicsRenderPass(GraphicsRenderPassProps properties);
  ~GraphicsRenderPass();

  GraphicsRenderPassProps m_Properties;
  VkRenderPass m_Pass{};
};

// todo
enum class PassType {
  kPassTypeGeometry,
  kPassTypePostProcess
};
/*
class RenderPass {
 public:
  RenderPass(Ref<Shader> vertexShader, Ref<Shader> fragmentShader);

  void Bake();

  void BeginFrame();
  void EndFrame();
};
*/

}  // namespace Wiesel