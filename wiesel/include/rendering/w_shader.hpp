
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
// todo
enum ShaderType { ShaderTypeVertex, ShaderTypeFragment };

enum ShaderSource { ShaderSourcePrecompiled, ShaderSourceSource };

enum ShaderLang { ShaderLangGLSL, ShaderLangHLSL };

VkShaderStageFlagBits GetShaderFlagBitsByType(ShaderType type);

struct ShaderProperties {
  ShaderType type;
  ShaderLang lang;
  std::string main;
  ShaderSource source;
  std::string path;
  std::vector<std::string> defines;
};

struct Shader {
  Shader(ShaderProperties properties);
  ~Shader();

  VkShaderModule shader_module_;
  ShaderProperties properties_;
};

}  // namespace Wiesel