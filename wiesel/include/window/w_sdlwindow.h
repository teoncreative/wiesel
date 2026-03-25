
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "w_pch.h"

#include "window/w_window.h"

namespace Wiesel {
class SdlAppWindow : public AppWindow {
 public:
  explicit SdlAppWindow(const WindowProperties&& properties);
  ~SdlAppWindow();

  void OnUpdate() override;
  bool IsShouldClose() override;

  void ImGuiInit() override;
  void ImGuiNewFrame() override;

  void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) override;
  void GetWindowFramebufferSize(WindowSize& size) override;
  const char** GetRequiredInstanceExtensions(
      uint32_t* extensionsCount) override;

  void SetTitle(const std::string& title) override;
  void SetIcon(const uint8_t* pixels, int width, int height) override;
  void SetCursorMode(CursorMode cursor_mode) override;
  void WarpCursor(double x, double y) override;
  void GetCursorDelta(double& dx, double& dy) override;
  void ResetCursorDelta() override;

 private:
  static KeyCode TranslateKeyCode(SDL_Scancode scancode);

  WindowSize window_size_;
  WindowSize framebuffer_size_;
  WindowSize scale_;

  SDL_Window* handle_{};
  bool should_close_ = false;
  double prev_cursor_x_ = 0.0, prev_cursor_y_ = 0.0;
  bool cursor_relative_first_ = true;
  double cursor_delta_x_ = 0.0, cursor_delta_y_ = 0.0;

  // Gamepad state tracking
  struct GamepadState {
    SDL_Gamepad* gamepad = nullptr;
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
    std::array<float, SDL_GAMEPAD_AXIS_COUNT> axes{};
  };

  std::unordered_map<SDL_JoystickID, GamepadState> gamepads_;
};
}  // namespace Wiesel