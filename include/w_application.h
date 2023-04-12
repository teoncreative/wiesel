//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "w_pch.h"
//#ifdef DEBUG
//#define WIESEL_PROFILE 1
//#endif
#include "util/w_profiler.h"
#include "util/w_utils.h"
#include "events/w_events.h"
#include "events/w_appevents.h"
#include "layer/w_layer.h"
#include "layer/w_layerimgui.h"
#include "rendering/w_renderer.h"
#include "scene/w_scene.h"

namespace Wiesel {
	class Application {
	public:
		Application();
		virtual ~Application();

		virtual void Init() = 0;

		void Run();
		void Close();

		void OnEvent(Event& event);

		void PushLayer(const Reference<Layer>& layer);
		void RemoveLayer(const Reference<Layer>& layer);

		void PushOverlay(const Reference<Layer>& layer);
		void RemoveOverlay(const Reference<Layer>& layer);

		bool OnWindowClose(WindowCloseEvent& event);
		bool OnWindowResize(WindowResizeEvent& event);

		WIESEL_GETTER_FN Reference<AppWindow> GetWindow();
		WIESEL_GETTER_FN const WindowSize& GetWindowSize();
		WIESEL_GETTER_FN Reference<Scene> GetScene();

	protected:
		bool m_IsRunning;
		bool m_IsMinimized;
		bool m_WindowResized;
		WindowSize m_WindowSize;
		std::vector<Reference<Layer>> m_Layers;
		std::vector<Reference<Layer>> m_Overlays; // maybe have another class extending from Layer like OverlayLayer
		Reference<ImGuiLayer> m_ImGuiLayer;
		uint32_t m_LayerCounter;
		Reference<AppWindow> m_Window;
		float_t m_PreviousFrame = 0.0;
		float_t m_DeltaTime = 0.0;
		Reference<Scene> m_Scene;
	};

}
