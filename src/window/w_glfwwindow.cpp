
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

namespace Wiesel {
	GlfwAppWindow::GlfwAppWindow(WindowProperties& properties) : AppWindow(properties) {

	}

	GlfwAppWindow::~GlfwAppWindow() {
		glfwDestroyWindow(m_Handle);
		glfwTerminate();
	}

	void GlfwAppWindow::Init() {
		glfwInit();
		LogDebug("GLFW Vulkan Support: " + std::to_string(glfwVulkanSupported()));
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		if (m_Properties.Resizable) {
			glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		}

		m_Handle = glfwCreateWindow(m_Properties.Size.Width, m_Properties.Size.Height, m_Properties.Title.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(m_Handle, this);

		glfwSetWindowSizeCallback(m_Handle, [](GLFWwindow* window, int width, int height) {
			GlfwAppWindow& appWindow = *(GlfwAppWindow*) glfwGetWindowUserPointer(window);
			WindowSize size{width, height};
			appWindow.OnFramebufferResize(size);
			// todo resize event
		});

		glfwSetWindowCloseCallback(m_Handle, [](GLFWwindow* window) {
			// todo close event
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

			MouseMovedEvent event((float) xPos, (float) yPos);
			appWindow.GetEventHandler()(event);
		});
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

	void GlfwAppWindow::OnFramebufferResize(const WindowSize& size) {
		SetFramebufferResized(true);
	}

}