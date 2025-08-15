//
// Created by Metehan Gezer on 28/07/2025.
//

#ifndef WIESEL_PARENT_W_GPU_DEBUG_HPP
#define WIESEL_PARENT_W_GPU_DEBUG_HPP

#include <array>
#include "util/w_color.hpp"
#include "vulkan/vulkan_core.h"

namespace Wiesel {

class PerfMarker {
 public:
  static inline PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT =
      nullptr;
  static inline PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT =
      nullptr;

  static void Init(VkInstance instance) {
    vkCmdBeginDebugUtilsLabelEXT =
        (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(
            instance, "vkCmdBeginDebugUtilsLabelEXT");
    vkCmdEndDebugUtilsLabelEXT =
        (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(
            instance, "vkCmdEndDebugUtilsLabelEXT");
  }
};

class ScopedPerfMarker {
 public:
  ScopedPerfMarker(VkCommandBuffer cmd, const char* name, Colorf color)
      : m_Cmd(cmd), m_EndFunc(nullptr) {
    if (!PerfMarker::vkCmdBeginDebugUtilsLabelEXT ||
        !PerfMarker::vkCmdEndDebugUtilsLabelEXT)
      return;

    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    label.color[0] = color.Red;
    label.color[1] = color.Green;
    label.color[2] = color.Blue;
    label.color[3] = color.Alpha;
    PerfMarker::vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
    m_EndFunc = vkCmdEndDebugUtilsLabelEXT;
  }

  ~ScopedPerfMarker() {
    if (m_EndFunc)
      m_EndFunc(m_Cmd);
  }

 private:
  VkCommandBuffer m_Cmd;
  PFN_vkCmdEndDebugUtilsLabelEXT m_EndFunc;
};

}  // namespace Wiesel
#endif  //WIESEL_PARENT_W_GPU_DEBUG_HPP
