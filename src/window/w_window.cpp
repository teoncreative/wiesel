
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "window/w_window.hpp"

namespace Wiesel {

  AppWindow::AppWindow(const WindowProperties& properties) : m_Properties(properties) {
  }

  AppWindow::~AppWindow() {
  }

  void AppWindow::SetEventHandler(const WindowEventFn& eventHandler) {
    m_EventHandler = eventHandler;
  }

  WindowEventFn& AppWindow::GetEventHandler() {
    return m_EventHandler;
  }

  void AppWindow::SetCursorMode(CursorMode mouseMode) {
    m_CursorMode = mouseMode;
  }

  CursorMode AppWindow::GetCursorMode() {
    return m_CursorMode;
  }

  void AppWindow::ImGuiInit() {
  }

  void AppWindow::ImGuiNewFrame() {
  }

}// namespace Wiesel
