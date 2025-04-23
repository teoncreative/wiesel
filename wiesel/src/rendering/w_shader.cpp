
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_shader.hpp"
#include "util/w_spirv.hpp"

#include "w_engine.hpp"

namespace Wiesel {

VkShaderStageFlagBits GetShaderFlagBitsByType(ShaderType type) {
  switch (type) {
    case ShaderTypeVertex:
      return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderTypeFragment:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    default:
      // Invalid
      return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
  }
}

Shader::Shader(ShaderProperties properties) : m_Properties(properties) {
  std::vector<uint32_t> code{};
  if (m_Properties.Source == ShaderSourceSource) {
    auto file = ReadFile(m_Properties.Path);
    if (!Spirv::ShaderToSPV(m_Properties.Type, file, code)) {
      throw std::runtime_error("Failed to compile shader!");
    }
  } else if (m_Properties.Source == ShaderSourcePrecompiled) {
    code = ReadFileUint32(m_Properties.Path);
  } else {
    throw std::runtime_error("Shader source not implemented!");
  }
  LOG_DEBUG("Creating shader with lang: {}, type: {}, source: {} {}, main: {}",
            std::to_string(m_Properties.Lang), std::to_string(m_Properties.Type),
            std::to_string(m_Properties.Source), m_Properties.Path,
            m_Properties.Main);
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size() * 4;
  createInfo.pCode = code.data();
  WIESEL_CHECK_VKRESULT(vkCreateShaderModule(Engine::GetRenderer()->GetLogicalDevice(), &createInfo,
                                             nullptr, &m_ShaderModule));
}

Shader::~Shader() {
  vkDestroyShaderModule(Engine::GetRenderer()->GetLogicalDevice(), m_ShaderModule, nullptr);
}

}  // namespace Wiesel