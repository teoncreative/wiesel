
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_renderpipeline.hpp"

#include "w_engine.hpp"

namespace Wiesel {

GraphicsPipeline::GraphicsPipeline(PipelineProperties properties)
    : m_Properties(properties) {
  m_IsAllocated = false;
}

GraphicsPipeline::~GraphicsPipeline() {
  m_Properties.m_RenderPass = nullptr;
  m_Properties.m_DescriptorLayout = nullptr;
  m_Properties.m_VertexShader = nullptr;
  m_Properties.m_FragmentShader = nullptr;
  Engine::GetRenderer()->DestroyGraphicsPipeline(*this);
}

void GraphicsPipeline::Bind(PipelineBindPoint bindPoint) {
  vkCmdBindPipeline(Engine::GetRenderer()->GetCommandBuffer().m_Handle, ToVkPipelineBindPoint(bindPoint),
                    m_Pipeline);
}

}  // namespace Wiesel