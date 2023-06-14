
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

#include "w_engine.hpp"

namespace Wiesel {

  Shader::Shader(ShaderProperties properties) : Properties(properties) {
  }

  Shader::~Shader() {
    Engine::GetRenderer()->DestroyShader(*this);
  }

}// namespace Wiesel