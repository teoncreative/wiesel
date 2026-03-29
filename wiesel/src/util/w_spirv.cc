
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_spirv.h"

#include "glslang/Public/ResourceLimits.h"
#include "util/w_logger.h"

namespace Wiesel::Spirv {

void Init() {
  LOG_DEBUG("Initializing glslang");
  glslang::InitializeProcess();
}

void Cleanup() {
  LOG_DEBUG("Cleaning up glslang");
  glslang::FinalizeProcess();
}

void InitResources(TBuiltInResource& resources) {
  resources = *GetDefaultResources();
}

EShLanguage FindLanguage(ShaderType type) {
  switch (type) {
    case ShaderTypeVertex:
      return EShLangVertex;
    case ShaderTypeFragment:
      return EShLangFragment;
    case ShaderTypeRayGen:
      return EShLangRayGen;
    case ShaderTypeClosestHit:
      return EShLangClosestHit;
    case ShaderTypeMiss:
      return EShLangMiss;
    case ShaderTypeAnyHit:
      return EShLangAnyHit;
    case ShaderTypeIntersection:
      return EShLangIntersect;
    case ShaderTypeCallable:
      return EShLangCallable;
    default:
      throw std::runtime_error("Shader stage is not implemented yet");
  }
}

bool ShaderToSPV(ShaderType type, bool debug, const std::vector<char>& input,
                 const std::vector<std::string>& defines,
                 std::vector<uint32_t>& output) {
  LOG_INFO("Compiling shader...");
  EShLanguage stage = FindLanguage(type);
  glslang::TShader shader(stage);
  glslang::TProgram program;
  TBuiltInResource resources = {};
  InitResources(resources);

  shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
  shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_4);
  shader.setDebugInfo(debug);

  std::string preamble;
  for (auto& d : defines) {
    preamble += "#define " + d + "\n";
  }
  shader.setPreamble(preamble.c_str());

  EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
  const char* strings[] = {input.data()};
  const int lengths[] = {static_cast<int>(input.size())};
  shader.setStringsWithLengths(strings, lengths, std::size(strings));

  if (!shader.parse(&resources, 450, true, messages)) {
    puts(shader.getInfoLog());
    puts(shader.getInfoDebugLog());
    fflush(stdout);
    return false;
  }

  program.addShader(&shader);

  //
  // Program-level processing...
  //

  if (!program.link(messages)) {
    puts(shader.getInfoLog());
    puts(shader.getInfoDebugLog());
    fflush(stdout);
    return false;
  }
  glslang::SpvOptions opt{};
  opt.validate = true;
  if (!debug) {
    opt.stripDebugInfo = true;
  }
  glslang::GlslangToSpv(*program.getIntermediate(stage), output, &opt);
  return true;
}

}  // namespace Wiesel::Spirv