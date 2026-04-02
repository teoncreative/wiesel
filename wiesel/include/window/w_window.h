
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

#include <utility>

#include "events/w_events.h"
#include "util/w_logger.h"
#include "util/w_utils.h"
#include "w_pch.h"

namespace Wiesel {
using WindowEventFn = std::function<void(Event&)>;

struct WindowSize {
  int32_t width;
  int32_t height;
};

enum CursorMode : uint8_t {
  CursorModeNormal,    // Cursor visible, absolute position
  CursorModeHidden,    // Cursor hidden but constrained, absolute position
  CursorModeRelative,  // Cursor hidden and unlocked, sends delta
  CursorModeUnlocked   // Cursor hidden and unlocked, absolute position
};

// Identifies who requested cursor capture, so the event loop can decide
// whether to block editor UI (ImGui) events.
enum class CursorCaptureSource : uint8_t {
  None,    // No capture active
  Editor,  // Editor viewport right-click look
  Game,    // Game script via Input.SetCursorMode
};

struct WindowProperties {
  std::string title;
  WindowSize size;
  bool resizable;

  WindowProperties(std::string title = "Wiesel",
                   const WindowSize& size = {1600, 900}, bool resizable = true)
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

  virtual void SetTitle(const std::string& title);
  virtual void SetIcon(const uint8_t* pixels, int width, int height);

  virtual void SetCursorMode(CursorMode mouse_mode);
  WIESEL_GETTER_FN virtual CursorMode GetCursorMode();

  void SetCursorCaptureSource(CursorCaptureSource source) {
    cursor_capture_source_ = source;
  }

  WIESEL_GETTER_FN CursorCaptureSource GetCursorCaptureSource() const {
    return cursor_capture_source_;
  }

  virtual void WarpCursor(double x, double y);

  // Custom cursor: set from RGBA pixel data. Pass nullptr to reset to default.
  virtual void SetCustomCursor(const uint8_t* pixels, int width, int height,
                               int hotspot_x, int hotspot_y) {}

  virtual void ResetCustomCursor() {}

  // Keyboard modifier state
  virtual bool IsShiftDown() const { return false; }

  virtual bool IsCtrlDown() const { return false; }

  virtual bool IsAltDown() const { return false; }

  // Clipboard
  virtual void SetClipboardText(const std::string& text) {}

  virtual std::string GetClipboardText() { return ""; }

  virtual void GetCursorDelta(double& dx, double& dy) {
    dx = 0;
    dy = 0;
  }

  virtual void ResetCursorDelta() {}

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
  CursorCaptureSource cursor_capture_source_ = CursorCaptureSource::None;
};
}  // namespace Wiesel
