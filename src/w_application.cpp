
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_application.hpp"
#include "window/w_glfwwindow.hpp"
#include "w_engine.hpp"
#include "input/w_input.hpp"

namespace Wiesel {

	Application* Application::s_Application;

	Application::Application(WindowProperties props) {
		WIESEL_PROFILE_FUNCTION();
		s_Application = this;

		m_LayerCounter = 0;
		m_IsRunning = true;
		m_IsMinimized = false;

		Engine::InitWindow(props);
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
		dispatcher.Dispatch<KeyPressedEvent>(WIESEL_BIND_EVENT_FUNCTION(OnKeyPressed));
		dispatcher.Dispatch<KeyReleasedEvent>(WIESEL_BIND_EVENT_FUNCTION(OnKeyReleased));
		dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_EVENT_FUNCTION(OnMouseMoved));

		if (event.m_Handled) {
			return;
		}

		m_Scene->OnEvent(event);
		for(auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it) {
			const auto& layer = *it;
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

				if (!Engine::GetRenderer()->BeginFrame(m_Scene->GetPrimaryCamera())) {
					continue;
				}
				m_ImGuiLayer->OnBeginFrame();
				for (const auto& layer : m_Overlays) {
					layer->OnImGuiRender();
				}
				if (m_Scene->GetPrimaryCamera()) {
					m_Scene->Render();
				}
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
		LOG_INFO("Closing the application!");
		m_IsRunning = false;
	}

	bool Application::OnWindowClose(WindowCloseEvent& event) {
		Close();
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& event) {
		m_WindowResized = true;
		return false;
	}

	// todo handle input logic in InputManager
	void Application::UpdateKeyboardAxis() {
		bool right = InputManager::GetKey("Right");
		bool left = InputManager::GetKey("Left");
		bool up = InputManager::GetKey("Up");
		bool down = InputManager::GetKey("Down");

		if (right && !left) {
			InputManager::m_Axis["Horizontal"] = 1;
		} else if (!right && left) {
			InputManager::m_Axis["Horizontal"] = -1;
		} else {
			InputManager::m_Axis["Horizontal"] = 0;
		}

		if (up && !down) {
			InputManager::m_Axis["Vertical"] = 1;
		} else if (!up && down) {
			InputManager::m_Axis["Vertical"] = -1;
		} else {
			InputManager::m_Axis["Vertical"] = 0;
		}
	}

	bool Application::OnKeyPressed(Wiesel::KeyPressedEvent& event) {
		InputManager::m_InputMode = InputModeKeyboardAndMouse;
		InputManager::m_Keys[event.GetKeyCode()].Pressed = true;
		UpdateKeyboardAxis();
		return false;
	}

	bool Application::OnKeyReleased(Wiesel::KeyReleasedEvent& event) {
		InputManager::m_Keys[event.GetKeyCode()].Pressed = false;
		UpdateKeyboardAxis();
		return false;
	}

	bool Application::OnMouseMoved(Wiesel::MouseMovedEvent& event) {
		InputManager::m_InputMode = InputModeKeyboardAndMouse;
		InputManager::m_MouseX = event.GetX();
		InputManager::m_MouseY = event.GetY();
		// todo mouse delta raw
		if (event.GetCursorMode() == CursorModeRelative) {
			InputManager::m_Axis["Mouse X"] += InputManager::m_MouseAxisSensX * (((m_WindowSize.Width / 2.0f) - event.GetX()) / m_WindowSize.Width);
			InputManager::m_Axis["Mouse Y"] += InputManager::m_MouseAxisSensY * (((m_WindowSize.Height / 2.0f) - event.GetY()) / m_WindowSize.Width);
			InputManager::m_Axis["Mouse Y"] = std::clamp(InputManager::m_Axis["Mouse Y"], -InputManager::m_MouseAxisLimitY, InputManager::m_MouseAxisLimitY);
		}
		return false;
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