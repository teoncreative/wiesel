
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

#include <utility>

#include "events/w_events.hpp"
#include "util/w_logger.hpp"
#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {
using WindowEventFn = std::function<void(Event&)>;

struct WindowSize {
  int32_t Width;
  int32_t Height;
};

enum CursorMode : uint8_t { CursorModeNormal, CursorModeRelative };

struct WindowProperties {
  std::string title;
  WindowSize size;
  bool resizable;

  WindowProperties(std::string title = "Wiesel",
                   const WindowSize& size = {1600, 900}, bool resizable = false)
      : title(std::move(title)), size(size), resizable(resizable) {}
};

class AppWindow {
 public:
  explicit AppWindow(const WindowProperties& properties);
  ~AppWindow();

  virtual void OnUpdate() = 0;
  virtual bool IsShouldClose() = 0;

  virtual void ImGuiInit();
  virtual void ImGuiNewFrame();

  void SetEventHandler(const WindowEventFn& callback);
  WIESEL_GETTER_FN WindowEventFn& GetEventHandler();

  virtual void SetCursorMode(CursorMode mouse_mode);
  WIESEL_GETTER_FN virtual CursorMode GetCursorMode();

  virtual void CreateWindowSurface(VkInstance instance,
                                   VkSurfaceKHR* surface) = 0;
  virtual void GetWindowFramebufferSize(WindowSize& size) = 0;
  virtual const char** GetRequiredInstanceExtensions(
      uint32_t* extensionsCount) = 0;

 protected:
  friend class Input;

  WindowProperties properties_;
  WindowEventFn event_handler_;
  CursorMode cursor_mode_;
};
}  // namespace Wiesel
