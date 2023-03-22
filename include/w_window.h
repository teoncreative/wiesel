//
// Created by Metehan Gezer on 20.03.2023.
//

#ifndef WIESEL_W_WINDOW_H
#define WIESEL_W_WINDOW_H

#include "w_utils.h"
#include "w_logger.h"
#include <GLFW/glfw3.h>

typedef void (*WindowEventKeyPress)(int key, int scancode, int action, int mods);
#define WINDOW_KEY_RELEASE 0
#define WINDOW_KEY_PRESS 1
#define WINDOW_KEY_REPEAT 2

class WieselWindow {
public:
	WieselWindow(int width, int height, const char* title);
	~WieselWindow();

	void init();

	void onFramebufferResize(int width, int height);

	bool isFramebufferResized();
	void setFramebufferResized(bool value);

	void setEventKeyPress(WindowEventKeyPress functionPtr);

	WindowEventKeyPress getEventKeyPress() const;

	bool isShouldClose();

	GLFWwindow* handle;
private:
	int width, height;
	const char* title;
	bool framebufferresized;
	WindowEventKeyPress eventKeyPress;
};


#endif //WIESEL_W_WINDOW_H
