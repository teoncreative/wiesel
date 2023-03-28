//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "window/w_window.h"

namespace Wiesel {
	AppWindow::AppWindow(WindowProperties& properties) : m_Properties(properties) {

	}

	AppWindow::~AppWindow() {
	}

	bool AppWindow::IsFramebufferResized() {
		return m_FrameBufferResized;
	}

	void AppWindow::SetFramebufferResized(bool value) {
		this->m_FrameBufferResized = value;
	}

	void AppWindow::SetEventHandler(const WindowEventFn& eventHandler) {
		m_EventHandler = eventHandler;
	}

	WindowEventFn& AppWindow::GetEventHandler() {
		return m_EventHandler;
	}
}
