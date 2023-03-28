//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "w_pch.h"
#include "w_object.h"
#include "util/w_utils.h"
#include "events/w_events.h"
#include "w_layer.h"

namespace Wiesel {
	class Application {
	public:
		Application();
		virtual ~Application();

		virtual void Init() = 0;

		void Run();

		void OnEvent(Event& event);

		void PushLayer(const SharedPtr<Layer>& layer);

		void RemoveLayer(const SharedPtr<Layer>& layer);
	private:
		bool m_IsRunning;
		std::vector<SharedPtr<Layer>> m_Layers;
		uint32_t m_LayerCounter;
		std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::high_resolution_clock::now();
		std::chrono::time_point<std::chrono::steady_clock> frameTime = std::chrono::high_resolution_clock::now();
		double_t totalTime = 0.0;
		double_t deltaTime = 0.0;
	};

	Application* CreateApp();
}
