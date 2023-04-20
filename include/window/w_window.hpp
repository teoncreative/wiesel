//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <utility>

#include "w_pch.hpp"
#include "util/w_utils.hpp"
#include "util/w_logger.hpp"
#include "events/w_events.hpp"

namespace Wiesel {
	using WindowEventFn = std::function<void(Event&)>;

	struct WindowSize {
		int32_t Width;
		int32_t Height;
	};

	enum CursorMode {
		CursorModeNormal,
		CursorModeRelative
	};

	struct WindowProperties {
		std::string Title;
		WindowSize Size;
		bool Resizable;

		explicit WindowProperties(std::string title = "Wiesel",
								  const WindowSize& size = {1600, 900},
								  bool resizable = false)
				: Title(std::move(title)), Size(size), Resizable(resizable) {
		}
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

		virtual void SetCursorMode(CursorMode mouseMode);
		WIESEL_GETTER_FN virtual CursorMode GetCursorMode();

		virtual void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) = 0;
		virtual void GetWindowFramebufferSize(WindowSize& size) = 0;
		virtual const char** GetRequiredInstanceExtensions(uint32_t* extensionsCount) = 0;

	protected:
		friend class Input;

		WindowProperties m_Properties;
		WindowEventFn m_EventHandler;
		CursorMode m_CursorMode;
	};
}
