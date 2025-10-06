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
      : cmd_(cmd), end_func_(nullptr) {
    if (!PerfMarker::vkCmdBeginDebugUtilsLabelEXT ||
        !PerfMarker::vkCmdEndDebugUtilsLabelEXT)
      return;

    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    label.color[0] = color.red;
    label.color[1] = color.green;
    label.color[2] = color.blue;
    label.color[3] = color.alpha;
    PerfMarker::vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
    end_func_ = vkCmdEndDebugUtilsLabelEXT;
  }

  ~ScopedPerfMarker() {
    if (end_func_)
      end_func_(cmd_);
  }

 private:
  VkCommandBuffer cmd_;
  PFN_vkCmdEndDebugUtilsLabelEXT end_func_;
};

}  // namespace Wiesel
#endif  //WIESEL_PARENT_W_GPU_DEBUG_HPP
