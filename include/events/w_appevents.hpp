
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
#include "events/w_events.hpp"
#include "util/w_mousecodes.hpp"
#include "window/w_window.hpp"

namespace Wiesel {
	class WindowCloseEvent : public Event
	{
	public:
		WindowCloseEvent() { }

		EVENT_CLASS_TYPE(WindowClose)
		EVENT_CLASS_CATEGORY(EventCategory::App)
	private:
	};

	class WindowResizeEvent : public Event
	{
	public:
		WindowResizeEvent(WindowSize windowSize, float_t aspectRatio) : m_WindowSize(windowSize), m_AspectRatio(aspectRatio) { }

		WIESEL_GETTER_FN const WindowSize& GetWindowSize() { return m_WindowSize; }
		WIESEL_GETTER_FN float GetAspectRatio() const { return m_AspectRatio; }

		EVENT_CLASS_TYPE(WindowResize)
		EVENT_CLASS_CATEGORY(EventCategory::App)
	private:
		WindowSize m_WindowSize;
		float_t m_AspectRatio;

	};

	class AppRecreateSwapChainsEvent : public Event
	{
	public:
		AppRecreateSwapChainsEvent(WindowSize windowSize, float_t aspectRatio) : m_WindowSize(windowSize), m_AspectRatio(aspectRatio) { }

		WIESEL_GETTER_FN const WindowSize& GetWindowSize() { return m_WindowSize; }
		WIESEL_GETTER_FN float GetAspectRatio() const { return m_AspectRatio; }

		EVENT_CLASS_TYPE(AppRecreateSwapChains)
		EVENT_CLASS_CATEGORY(EventCategory::App)
	private:
		WindowSize m_WindowSize;
		float_t m_AspectRatio;

	};
}
