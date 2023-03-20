//
// Created by Metehan Gezer on 20.03.2023.
//

#ifndef WIESEL_WIESELWINDOW_H
#define WIESEL_WIESELWINDOW_H

#include "Utils.h"
#include "Logger.h"
#include <GLFW/glfw3.h>

static void callbackFramebufferResize(GLFWwindow* window, int width, int height);

class WieselWindow {
public:
	WieselWindow(int width, int height, const char* title);
	~WieselWindow();

	void init();

	void onFramebufferResize(int width, int height);

	bool isFramebufferResized();
	void setFramebufferResized(bool value);

	bool isShouldClose();

	GLFWwindow* handle;
private:
	int width, height;
	const char* title;
	bool framebufferresized;
};


#endif //WIESEL_WIESELWINDOW_H
