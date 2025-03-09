
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
#include <SPIRV/GlslangToSpv.h>
#include "glslang/Include/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"
#include "rendering/w_shader.hpp"

namespace Wiesel::Spirv {
void Init();
void Cleanup();
void InitResources(TBuiltInResource& resources);
EShLanguage FindLanguage(ShaderType type);
bool ShaderToSPV(ShaderType type, const std::vector<char>& input,
                 std::vector<uint32_t>& output);
}  // namespace Wiesel::Spirv