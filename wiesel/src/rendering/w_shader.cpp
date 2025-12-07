
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

Shader::Shader(ShaderProperties properties) : properties_(properties) {
  std::vector<uint32_t> code{};
  if (properties_.source == ShaderSourceSource) {
    auto file = ReadFile(properties_.path);
#ifdef DEBUG
    bool debug = true;
#else
    bool debug = false;
#endif
    if (!Spirv::ShaderToSPV(properties_.type, debug, file, properties_.defines, code)) {
      throw std::runtime_error("Failed to compile shader!");
    }
  } else if (properties_.source == ShaderSourcePrecompiled) {
    if (!properties.defines.empty()) {
      LOG_WARN("Defines for shader was not empty but the shader is precompiled. Defines might not be matching.");
    }
    code = ReadFileUint32(properties_.path);
  } else {
    throw std::runtime_error("Shader source not implemented!");
  }
  LOG_DEBUG("Creating shader with lang: {}, type: {}, source: {} {}, main: {}",
            std::to_string(properties_.lang), std::to_string(properties_.type),
            std::to_string(properties_.source), properties_.path,
            properties_.main);
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size() * 4;
  createInfo.pCode = code.data();
  WIESEL_CHECK_VKRESULT(vkCreateShaderModule(Engine::GetRenderer()->GetLogicalDevice(), &createInfo,
                                             nullptr, &shader_module_));
}

Shader::~Shader() {
  vkDestroyShaderModule(Engine::GetRenderer()->GetLogicalDevice(), shader_module_, nullptr);
}

}  // namespace Wiesel