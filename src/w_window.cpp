//
// Created by Metehan Gezer on 20.03.2023.
//

#include "w_window.h"


static void callbackFramebufferResize(GLFWwindow* window, int width, int height);
static void callbackKey(GLFWwindow* window, int key, int scancode, int action, int mods);

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
	wge::logDebug("glfw Vulkan Support: " + std::to_string(glfwVulkanSupported()));
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
	glfwSetWindowUserPointer(handle, this);
	glfwSetFramebufferSizeCallback(handle, callbackFramebufferResize);
	glfwSetKeyCallback(handle, callbackKey);
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

void WieselWindow::setEventKeyPress(WindowEventKeyPress functionPtr) {
	eventKeyPress = functionPtr;
}

WindowEventKeyPress WieselWindow::getEventKeyPress() const {
	return eventKeyPress;
}

static void callbackFramebufferResize(GLFWwindow* window, int width, int height) {
	auto* w = reinterpret_cast<WieselWindow*>(glfwGetWindowUserPointer(window));
	w->onFramebufferResize(width, height);
}

static void callbackKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
	auto* w = reinterpret_cast<WieselWindow*>(glfwGetWindowUserPointer(window));
	w->getEventKeyPress()(key, scancode, action, mods);
}
