
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "window/w_glfwwindow.h"
#include "events/w_keyevents.h"
#include "events/w_mouseevents.h"
#include "events/w_appevents.h"
#include "backends/imgui_impl_glfw.h"

namespace Wiesel {
	GlfwAppWindow::GlfwAppWindow(const WindowProperties& properties) : AppWindow(properties) {
		glfwInit();
		LOG_DEBUG("GLFW Vulkan Support: " + std::to_string(glfwVulkanSupported()));
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		if (m_Properties.Resizable) {
			glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		}

		m_Handle = glfwCreateWindow(m_Properties.Size.Width, m_Properties.Size.Height, m_Properties.Title.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(m_Handle, this);

		glfwGetFramebufferSize(m_Handle, &m_FramebufferSize.Width, &m_FramebufferSize.Height);
		glfwGetWindowSize(m_Handle, &m_WindowSize.Width, &m_WindowSize.Height);
		m_Scale.Width = m_FramebufferSize.Width / (float) m_WindowSize.Width;
		m_Scale.Height = m_FramebufferSize.Height / (float) m_WindowSize.Height;

		glfwSetWindowSizeCallback(m_Handle, [](GLFWwindow* window, int width, int height) {
			GlfwAppWindow& appWindow = *(GlfwAppWindow*) glfwGetWindowUserPointer(window);

			glfwGetFramebufferSize(appWindow.m_Handle, &appWindow.m_FramebufferSize.Width, &appWindow.m_FramebufferSize.Height);
			glfwGetWindowSize(appWindow.m_Handle, &appWindow.m_WindowSize.Width, &appWindow.m_WindowSize.Height);
			appWindow.m_Scale.Width = appWindow.m_FramebufferSize.Width / (float) appWindow.m_WindowSize.Width;
			appWindow.m_Scale.Height = appWindow.m_FramebufferSize.Height / (float) appWindow.m_WindowSize.Height;

			WindowResizeEvent event({width, height});
			appWindow.GetEventHandler()(event);
		});

		glfwSetWindowCloseCallback(m_Handle, [](GLFWwindow* window) {
			GlfwAppWindow& appWindow = *(GlfwAppWindow*) glfwGetWindowUserPointer(window);

			WindowCloseEvent event;
			appWindow.GetEventHandler()(event);
		});

		glfwSetKeyCallback(m_Handle, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			GlfwAppWindow& appWindow = *(GlfwAppWindow*) glfwGetWindowUserPointer(window);

			switch (action) {
				case GLFW_PRESS: {
					KeyPressedEvent event(key, false);
					appWindow.GetEventHandler()(event);
					break;
				}
				case GLFW_RELEASE: {
					KeyReleasedEvent event(key);
					appWindow.GetEventHandler()(event);
					break;
				}
				case GLFW_REPEAT: {
					KeyPressedEvent event(key, true);
					appWindow.GetEventHandler()(event);
					break;
				}
			}
		});

		glfwSetCharCallback(m_Handle, [](GLFWwindow* window, unsigned int keycode) {
			GlfwAppWindow& appWindow = *(GlfwAppWindow*) glfwGetWindowUserPointer(window);

			KeyTypedEvent event(keycode);
			appWindow.GetEventHandler()(event);
		});

		glfwSetMouseButtonCallback(m_Handle, [](GLFWwindow* window, int button, int action, int mods) {
			GlfwAppWindow& appWindow = *(GlfwAppWindow*) glfwGetWindowUserPointer(window);

			switch (action) {
				case GLFW_PRESS: {
					MouseButtonPressedEvent event(button);
					appWindow.GetEventHandler()(event);
					break;
				}
				case GLFW_RELEASE: {
					MouseButtonReleasedEvent event(button);
					appWindow.GetEventHandler()(event);
					break;
				}
			}
		});

		glfwSetScrollCallback(m_Handle, [](GLFWwindow* window, double xOffset, double yOffset) {
			GlfwAppWindow& appWindow = *(GlfwAppWindow*) glfwGetWindowUserPointer(window);

			MouseScrolledEvent event((float) xOffset, (float) yOffset);
			appWindow.GetEventHandler()(event);
		});

		glfwSetCursorPosCallback(m_Handle, [](GLFWwindow* window, double xPos, double yPos) {
			GlfwAppWindow& appWindow = *(GlfwAppWindow*) glfwGetWindowUserPointer(window);

			if (xPos > appWindow.m_WindowSize.Width || yPos > appWindow.m_WindowSize.Height || xPos < 0 || yPos < 0) {
				return;
			}

			MouseMovedEvent event((float) xPos * appWindow.m_Scale.Width, (float) yPos * appWindow.m_Scale.Height);
			appWindow.GetEventHandler()(event);
			if (appWindow.m_CursorMode == CursorModeRelative) {
				glfwSetCursorPos(window, appWindow.m_WindowSize.Width / 2.0f, appWindow.m_WindowSize.Height / 2.0f);
			}
		});
	}

	GlfwAppWindow::~GlfwAppWindow() {
		LOG_DEBUG("Destroying GlfwAppWindow");
		glfwDestroyWindow(m_Handle);
	}

	void GlfwAppWindow::OnUpdate() {
		glfwPollEvents();
	}

	bool GlfwAppWindow::IsShouldClose() {
		return glfwWindowShouldClose(m_Handle);
	}

	void GlfwAppWindow::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
		WIESEL_CHECK_VKRESULT(glfwCreateWindowSurface(instance, m_Handle, nullptr, surface));
	}

	void GlfwAppWindow::GetWindowFramebufferSize(WindowSize& size) {
		glfwGetFramebufferSize(m_Handle, &size.Width, &size.Height);
	}

	void GlfwAppWindow::SetCursorMode(CursorMode cursorMode) {
		m_CursorMode = cursorMode;
		switch (cursorMode) {
			case CursorModeNormal: {
				glfwSetInputMode(m_Handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				break;
			}
			case CursorModeRelative: {
				glfwSetInputMode(m_Handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				glfwSetCursorPos(m_Handle, m_WindowSize.Width / 2.0f, m_WindowSize.Height / 2.0f);
				break;
			}
		}
	}

	const char** GlfwAppWindow::GetRequiredInstanceExtensions(uint32_t* extensionsCount) {
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(extensionsCount);
		return glfwExtensions;
	}

	void GlfwAppWindow::ImGuiInit() {
		ImGui_ImplGlfw_InitForVulkan(m_Handle, true);
	}

	void GlfwAppWindow::ImGuiNewFrame() {
		ImGui_ImplGlfw_NewFrame();
	}

	float_t Time::GetTime() {
		return glfwGetTime();
	}
}