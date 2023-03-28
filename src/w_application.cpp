//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_application.h"

namespace Wiesel {
	Application::Application() {
		m_LayerCounter = 0;
		m_IsRunning = true;
	}

	Application::~Application() {
		for (const auto& item : m_Layers) {
			item->OnDetach();
		}
		m_Layers.clear();
	}

	void Application::OnEvent(Event& event) {
		EventDispatcher dispatcher(event);

//		dispatcher.Dispatch<WindowCloseEvent>(todo)

		for (const auto& layer : m_Layers) {
			if (event.m_Handled) {
				break;
			}

			layer->OnEvent(event);
		}
	}

	void Application::PushLayer(const SharedPtr<Layer>& layer) {
		m_Layers.push_back(layer);
		layer->OnAttach();
		layer->m_Id = m_LayerCounter++;
	}

	void Application::RemoveLayer(const SharedPtr<Layer>& layer) {
		// todo
	}

	void Application::Run() {
		while (m_IsRunning) {
			auto currentTime = std::chrono::high_resolution_clock::now();
			totalTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
			deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - frameTime).count();
			frameTime = currentTime;

			for (const auto& layer : m_Layers) {
				layer->OnUpdate(deltaTime);
			}

//			std::this_thread::sleep_for(std::chrono::milliseconds(15));
		}
	}

}