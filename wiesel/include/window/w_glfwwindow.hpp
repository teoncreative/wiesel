
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

#include "w_pch.hpp"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "window/w_window.hpp"

static void CallbackFramebufferResize(GLFWwindow* window, int width,
                                      int height);
static void CallbackKey(GLFWwindow* window, int key, int scancode, int action,
                        int mods);

namespace Wiesel {
class GlfwAppWindow : public AppWindow {
 public:
  explicit GlfwAppWindow(const WindowProperties&& properties);
  ~GlfwAppWindow();

  void OnUpdate() override;
  bool IsShouldClose() override;

  void ImGuiInit() override;
  void ImGuiNewFrame() override;

  void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) override;
  void GetWindowFramebufferSize(WindowSize& size) override;
  const char** GetRequiredInstanceExtensions(
      uint32_t* extensionsCount) override;

  void SetCursorMode(CursorMode cursorMode) override;

 private:
  WindowSize m_WindowSize;
  WindowSize m_FramebufferSize;
  WindowSize m_Scale;

  GLFWwindow* m_Handle{};
};
}  // namespace Wiesel
