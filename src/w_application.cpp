//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_pch.h"
#include "w_application.h"
#include "window/w_glfwwindow.h"

namespace Wiesel {
	Application::Application() {
		WIESEL_PROFILE_FUNCTION();
		m_LayerCounter = 0;
		m_IsRunning = true;
		m_IsMinimized = false;

		m_Window = CreateReference<GlfwAppWindow>(WindowProperties());
		m_Window->SetEventHandler(WIESEL_BIND_EVENT_FUNCTION(Application::OnEvent));

		Renderer::Create(m_Window);
	}

	Application::~Application() {
		Wiesel::LogDebug("Destroying Application");
		for (const auto& item : m_Layers) {
			item->OnDetach();
		}
		m_Layers.clear();
		Renderer::Destroy();
	}

	void Application::OnEvent(Event& event) {
		EventDispatcher dispatcher(event);

		dispatcher.Dispatch<WindowCloseEvent>(WIESEL_BIND_EVENT_FUNCTION(OnWindowClose));

		for (const auto& layer : m_Layers) {
			if (event.m_Handled) {
				break;
			}

			layer->OnEvent(event);
		}
	}

	void Application::PushLayer(const Reference<Layer>& layer) {
		m_Layers.push_back(layer);
		layer->OnAttach();
		layer->m_Id = m_LayerCounter++;
	}

	void Application::RemoveLayer(const Reference<Layer>& layer) {
		// todo
	}

	void Application::Run() {
		m_PreviousFrame = Time::GetTime();

		while (m_IsRunning) {
			double_t time = Time::GetTime();
			m_DeltaTime = time - m_PreviousFrame;
			m_PreviousFrame = time;

			if (!m_IsMinimized) {
				for (const auto& layer : m_Layers) {
					layer->OnUpdate(m_DeltaTime);
				}
			}
			m_Window->OnUpdate();

			Renderer::GetRenderer()->BeginFrame();
			Renderer::GetRenderer()->DrawMeshes();
			Renderer::GetRenderer()->EndFrame();
		}
	}

	void Application::Close() {
		m_IsRunning = false;
	}

	bool Application::OnWindowClose(WindowCloseEvent& event) {
		Close();
		return true;
	}
}