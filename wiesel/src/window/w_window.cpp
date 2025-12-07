
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

AppWindow::AppWindow(const WindowProperties& properties)
    : properties_(properties) {}

AppWindow::~AppWindow() {}

void AppWindow::SetEventHandler(const WindowEventFn& eventHandler) {
  event_handler_ = eventHandler;
}

WindowEventFn& AppWindow::GetEventHandler() {
  return event_handler_;
}

void AppWindow::SetCursorMode(CursorMode mouse_mode) {
  cursor_mode_ = mouse_mode;
}

CursorMode AppWindow::GetCursorMode() {
  return cursor_mode_;
}

void AppWindow::ImGuiInit() {}

void AppWindow::ImGuiNewFrame() {}

}  // namespace Wiesel
