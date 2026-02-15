
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_utils.hpp"
#include "w_engine.hpp"

namespace Wiesel {

std::string GetNameFromVulkanResult(VkResult errorCode) {
  switch (errorCode) {
#define STR(r) \
  case VK_##r: \
    return #r
    STR(NOT_READY);
    STR(TIMEOUT);
    STR(EVENT_SET);
    STR(EVENT_RESET);
    STR(INCOMPLETE);
    STR(ERROR_OUT_OF_HOST_MEMORY);
    STR(ERROR_OUT_OF_DEVICE_MEMORY);
    STR(ERROR_INITIALIZATION_FAILED);
    STR(ERROR_DEVICE_LOST);
    STR(ERROR_MEMORY_MAP_FAILED);
    STR(ERROR_LAYER_NOT_PRESENT);
    STR(ERROR_EXTENSION_NOT_PRESENT);
    STR(ERROR_FEATURE_NOT_PRESENT);
    STR(ERROR_INCOMPATIBLE_DRIVER);
    STR(ERROR_TOO_MANY_OBJECTS);
    STR(ERROR_FORMAT_NOT_SUPPORTED);
    STR(ERROR_SURFACE_LOST_KHR);
    STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
    STR(SUBOPTIMAL_KHR);
    STR(ERROR_OUT_OF_DATE_KHR);
    STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
    STR(ERROR_VALIDATION_FAILED_EXT);
    STR(ERROR_INVALID_SHADER_NV);
#undef STR
    default:
      return "UNKNOWN_ERROR";
  }
}

std::vector<char> ReadVirtualFile(const std::string& virtual_path) {
  VfsFile file = Engine::GetVirtualFileSystem()->Open(virtual_path);
  return file.AsChars();
}

std::vector<uint32_t> ReadVirtualFileUint32(const std::string& virtual_path) {
  VfsFile file = Engine::GetVirtualFileSystem()->Open(virtual_path);
  return file.AsUint32();
}

std::string FormatVariableName(const std::string& name) {
  // Calculate final size
  size_t final_size = name.size();
  for (char c : name) {
    if (c == '_') final_size--; // underscore becomes space, next char becomes uppercase
  }

  std::string result;
  result.reserve(final_size);

  for (size_t i = 0; i < name.size(); i++) {
    if (name[i] == '_' && i + 1 < name.size()) {
      result += ' ';
      result += std::toupper(name[i + 1]);
      i++;
    } else if (name[i] != '_') {
      result += name[i];
    }
  }

  return result;
}

}  // namespace Wiesel
