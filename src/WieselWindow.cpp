//
// Created by Metehan Gezer on 20.03.2023.
//

#include "WieselWindow.h"

WieselWindow::WieselWindow(int width, int height, const char* title) {
	this->width = width;
	this->height = height;
	this->title = title;
}

WieselWindow::~WieselWindow() {
	glfwDestroyWindow(handle);

	glfwTerminate();
}

void WieselWindow::init() {
	glfwInit();
	Wiesel::logDebug("glfw Vulkan Support: " + std::to_string(glfwVulkanSupported()));
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
	glfwSetWindowUserPointer(handle, this);
	glfwSetFramebufferSizeCallback(handle, callbackFramebufferResize);
}

void WieselWindow::onFramebufferResize(int width, int height) {
	setFramebufferResized(true);
}

bool WieselWindow::isFramebufferResized() {
	return framebufferresized;
}

void WieselWindow::setFramebufferResized(bool value) {
	this->framebufferresized = value;
}

bool WieselWindow::isShouldClose() {
	return glfwWindowShouldClose(handle);
}

static void callbackFramebufferResize(GLFWwindow* window, int width, int height) {
	WieselWindow* w = reinterpret_cast<WieselWindow*>(glfwGetWindowUserPointer(window));
	w->onFramebufferResize(width, height);
}
