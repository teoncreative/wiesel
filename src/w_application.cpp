//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_application.h"
#include "window/w_glfwwindow.h"
#include "w_engine.h"

namespace Wiesel {
	Application* Application::s_Application;

	Application::Application() {
		WIESEL_PROFILE_FUNCTION();
		s_Application = this;

		m_LayerCounter = 0;
		m_IsRunning = true;
		m_IsMinimized = false;

		Engine::InitWindow(WindowProperties());
		m_Window = Engine::GetWindow();
		m_Window->SetEventHandler(WIESEL_BIND_EVENT_FUNCTION(Application::OnEvent));

		m_Window->GetWindowFramebufferSize(m_WindowSize);
		if (m_WindowSize.Width == 0 || m_WindowSize.Height == 0) {
			m_IsMinimized = true;
		}

		Engine::InitRenderer();
		m_Scene = CreateReference<Scene>();
		m_ImGuiLayer = CreateReference<ImGuiLayer>();
		PushOverlay(m_ImGuiLayer);
	}

	Application::~Application() {
		LOG_DEBUG("Destroying Application");
		for (const auto& item : m_Overlays) {
			item->OnDetach();
		}
		m_Overlays.clear();
		for (const auto& item : m_Layers) {
			item->OnDetach();
		}
		m_Layers.clear();
		m_Scene = nullptr;
		m_Window = nullptr;
		Engine::CleanupRenderer();
		Engine::CleanupWindow();
	}

	void Application::OnEvent(Event& event) {
		EventDispatcher dispatcher(event);

		dispatcher.Dispatch<WindowCloseEvent>(WIESEL_BIND_EVENT_FUNCTION(OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(WIESEL_BIND_EVENT_FUNCTION(OnWindowResize));

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

	void Application::PushOverlay(const Reference<Layer>& layer) {
		m_Overlays.push_back(layer);
		layer->OnAttach();
		layer->m_Id = m_LayerCounter++;
	}

	void Application::RemoveOverlay(const Reference<Layer>& layer) {
		// todo
	}

	void Application::Run() {
		m_PreviousFrame = Time::GetTime();

		while (m_IsRunning) {
			float time = Time::GetTime();
			m_DeltaTime = time - m_PreviousFrame;
			m_PreviousFrame = time;

			ExecuteQueue();

			if (!m_IsMinimized) {
				for (const auto& layer : m_Layers) {
					layer->OnUpdate(m_DeltaTime);
				}
				for (const auto& layer : m_Overlays) {
					layer->OnUpdate(m_DeltaTime);
				}
				m_Scene->OnUpdate(m_DeltaTime);
				if (!Engine::GetRenderer()->BeginFrame()) {
					continue;
				}
				m_ImGuiLayer->OnBeginFrame();
				for (const auto& layer : m_Overlays) {
					layer->OnImGuiRender();
				}
				m_Scene->Render();
				for (const auto& layer : m_Overlays) {
					layer->PostRender();
				}
				m_ImGuiLayer->OnEndFrame();
                Engine::GetRenderer()->EndFrame();
            }

			m_Window->OnUpdate();

			if (m_WindowResized) {
				m_Window->GetWindowFramebufferSize(m_WindowSize);
				if (m_WindowSize.Width == 0 || m_WindowSize.Height == 0) {
					m_IsMinimized = true;
				} else {
					m_IsMinimized = false;
                    Engine::GetRenderer()->RecreateSwapChain();
				}
				m_WindowResized = false;
			}
		}
	}

	void Application::Close() {
		m_IsRunning = false;
	}

	bool Application::OnWindowClose(WindowCloseEvent& event) {
		Close();
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& event) {
		m_WindowResized = true;
		return true;
	}

	Reference<AppWindow> Application::GetWindow() {
		return m_Window;
	}

	const WindowSize& Application::GetWindowSize() {
		return m_WindowSize;
	}

	Reference<Scene> Application::GetScene() {
		return m_Scene;
	}

	void Application::SubmitToMainThread(std::function<void()> fn) {
		std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);
		m_MainThreadQueue.emplace_back(fn);
	}

	void Application::ExecuteQueue() {
		// this has to be inside its own scope or it might have problems
		std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);
		for (auto& func : m_MainThreadQueue) {
			func();
		}
		m_MainThreadQueue.clear();
	}

	Application* Application::Get() {
		return s_Application;
	}
}