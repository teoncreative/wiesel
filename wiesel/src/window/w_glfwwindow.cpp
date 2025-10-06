
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "window/w_glfwwindow.hpp"

#include <backends/imgui_impl_glfw.h>

#include "events/w_appevents.hpp"
#include "events/w_keyevents.hpp"
#include "events/w_mouseevents.hpp"

namespace Wiesel {

GlfwAppWindow::GlfwAppWindow(const WindowProperties&& properties)
    : AppWindow(properties) {
  glfwInit();
  LOG_DEBUG("GLFW Vulkan Support: {}", glfwVulkanSupported());
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  if (properties_.resizable) {
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  }

  handle_ = glfwCreateWindow(properties_.size.Width, properties_.size.Height,
                             properties_.title.c_str(), nullptr, nullptr);
  glfwSetWindowUserPointer(handle_, this);

  glfwGetFramebufferSize(handle_, &framebuffer_size_.Width,
                         &framebuffer_size_.Height);
  glfwGetWindowSize(handle_, &window_size_.Width, &window_size_.Height);
  scale_.Width = framebuffer_size_.Width / (float)window_size_.Width;
  scale_.Height = framebuffer_size_.Height / (float)window_size_.Height;

  glfwSetWindowSizeCallback(
      handle_, [](GLFWwindow* window, int width, int height) {
        GlfwAppWindow& appWindow =
            *(GlfwAppWindow*)glfwGetWindowUserPointer(window);

        glfwGetFramebufferSize(appWindow.handle_,
                               &appWindow.framebuffer_size_.Width,
                               &appWindow.framebuffer_size_.Height);
        glfwGetWindowSize(appWindow.handle_, &appWindow.window_size_.Width,
                          &appWindow.window_size_.Height);
        appWindow.scale_.Width = appWindow.framebuffer_size_.Width /
                                 (float)appWindow.window_size_.Width;
        appWindow.scale_.Height = appWindow.framebuffer_size_.Height /
                                  (float)appWindow.window_size_.Height;

        WindowResizeEvent event({width, height}, width / (float)height);
        appWindow.GetEventHandler()(event);
      });

  glfwSetWindowCloseCallback(handle_, [](GLFWwindow* window) {
    GlfwAppWindow& appWindow =
        *(GlfwAppWindow*)glfwGetWindowUserPointer(window);

    WindowCloseEvent event;
    appWindow.GetEventHandler()(event);
  });

  glfwSetKeyCallback(handle_, [](GLFWwindow* window, int key, int scancode,
                                 int action, int mods) {
    GlfwAppWindow& appWindow =
        *(GlfwAppWindow*)glfwGetWindowUserPointer(window);

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
  glfwSetCharCallback(handle_, [](GLFWwindow* window, unsigned int keycode) {
    GlfwAppWindow& appWindow =
        *(GlfwAppWindow*)glfwGetWindowUserPointer(window);

    KeyTypedEvent event(keycode);
    appWindow.GetEventHandler()(event);
  });

  glfwSetMouseButtonCallback(
      handle_, [](GLFWwindow* window, int button, int action, int mods) {
        GlfwAppWindow& appWindow =
            *(GlfwAppWindow*)glfwGetWindowUserPointer(window);

        switch (action) {
          case GLFW_PRESS: {
            MouseButtonPressedEvent event(static_cast<MouseCode>(button));
            appWindow.GetEventHandler()(event);
            break;
          }
          case GLFW_RELEASE: {
            MouseButtonReleasedEvent event(static_cast<MouseCode>(button));
            appWindow.GetEventHandler()(event);
            break;
          }
        }
      });

  glfwSetScrollCallback(
      handle_, [](GLFWwindow* window, double xOffset, double yOffset) {
        GlfwAppWindow& appWindow =
            *(GlfwAppWindow*)glfwGetWindowUserPointer(window);

        MouseScrolledEvent event((float)xOffset, (float)yOffset);
        appWindow.GetEventHandler()(event);
      });

  glfwSetCursorPosCallback(
      handle_, [](GLFWwindow* window, double xPos, double yPos) {
        GlfwAppWindow& appWindow =
            *(GlfwAppWindow*)glfwGetWindowUserPointer(window);

        if (xPos > appWindow.window_size_.Width ||
            yPos > appWindow.window_size_.Height || xPos < 0 || yPos < 0) {
          return;
        }

        MouseMovedEvent event((float)xPos * appWindow.scale_.Width,
                              (float)yPos * appWindow.scale_.Height,
                              appWindow.cursor_mode_);
        appWindow.GetEventHandler()(event);
        if (appWindow.cursor_mode_ == CursorModeRelative) {
          glfwSetCursorPos(window, appWindow.window_size_.Width / 2.0f,
                           appWindow.window_size_.Height / 2.0f);
        }
      });
}

GlfwAppWindow::~GlfwAppWindow() {
  LOG_DEBUG("Destroying GlfwAppWindow");
  glfwDestroyWindow(handle_);
}

void GlfwAppWindow::OnUpdate() {
  PROFILE_ZONE_SCOPED();
  glfwPollEvents();

  if (first_frame_) { [[unlikely]]
    glfwSetJoystickCallback([](int jid, int e) {
      auto& app =
          *(GlfwAppWindow*)glfwGetWindowUserPointer(glfwGetCurrentContext());
      if (e == GLFW_CONNECTED) {
        bool is_gamepad = glfwJoystickIsGamepad(jid);
        JoystickConnectedEvent ev(jid, "", is_gamepad);
        app.GetEventHandler()(ev);
      } else if (e == GLFW_DISCONNECTED) {
        JoystickDisconnectedEvent ev(jid);
        app.GetEventHandler()(ev);
        app.gamepad_prev_[jid].reset();
      }
    });
    for (int jid = 0; jid < GLFW_JOYSTICK_LAST; jid++) {
      bool is_gamepad = glfwJoystickIsGamepad(jid);
      JoystickConnectedEvent ev(jid, "", is_gamepad);
      GetEventHandler()(ev);
    }
    first_frame_ = false;
  }
  for (int joystick_id = 0; joystick_id <= GLFW_JOYSTICK_LAST; ++joystick_id) {
    if (!glfwJoystickPresent(joystick_id)) {
      continue;
    }

    if (glfwJoystickIsGamepad(joystick_id)) {
      GLFWgamepadstate current_state{};
      if (!glfwGetGamepadState(joystick_id, &current_state)) {
        continue;
      }

      auto& previous_state = gamepad_prev_[joystick_id];
      if (!previous_state.has_value()) {
        previous_state = current_state;
        continue;
      }

      for (int button_index = 0; button_index <= GLFW_GAMEPAD_BUTTON_LAST;
           ++button_index) {
        unsigned char previous_button = previous_state->buttons[button_index];
        unsigned char current_button = current_state.buttons[button_index];

        if (previous_button != current_button) {
          if (current_button == GLFW_PRESS) {
            JoystickButtonPressedEvent event(joystick_id, button_index);
            GetEventHandler()(event);
          } else {
            JoystickButtonReleasedEvent event(joystick_id, button_index);
            GetEventHandler()(event);
          }
        }
      }

      // axes
      for (int axis_index = 0; axis_index <= GLFW_GAMEPAD_AXIS_LAST;
           ++axis_index) {
        float previous_axis_value = previous_state->axes[axis_index];
        float current_axis_value = current_state.axes[axis_index];

        if (fabsf(current_axis_value - previous_axis_value) > 0.01f) {
          float adjusted_value =
              (fabsf(current_axis_value) < 0.15f) ? 0.0f : current_axis_value;
          JoystickAxisMovedEvent event(joystick_id, axis_index, adjusted_value);
          GetEventHandler()(event);
        }
      }

      previous_state = current_state;
    } else {
      int axis_count = 0;
      int button_count = 0;
      int hat_count = 0;

      const float* axis_values = glfwGetJoystickAxes(joystick_id, &axis_count);
      const unsigned char* button_values =
          glfwGetJoystickButtons(joystick_id, &button_count);
      const unsigned char* hat_values =
          glfwGetJoystickHats(joystick_id, &hat_count);

      auto& previous_state = joy_prev_[joystick_id];
      if (!previous_state.valid) {
        previous_state.valid = true;
        previous_state.axes.assign(axis_values, axis_values + axis_count);
        previous_state.buttons.assign(button_values,
                                      button_values + button_count);
        previous_state.hats.assign(hat_values, hat_values + hat_count);
        continue;
      }

      if ((int)previous_state.axes.size() != axis_count) {
        previous_state.axes.resize(axis_count, 0.0f);
      }
      if ((int)previous_state.buttons.size() != button_count) {
        previous_state.buttons.resize(button_count, GLFW_RELEASE);
      }
      if ((int)previous_state.hats.size() != hat_count) {
        previous_state.hats.resize(hat_count, GLFW_HAT_CENTERED);
      }

      // buttons
      for (int button_index = 0; button_index < button_count; ++button_index) {
        unsigned char previous_button = previous_state.buttons[button_index];
        unsigned char current_button = button_values[button_index];

        if (previous_button != current_button) {
          if (current_button == GLFW_PRESS) {
            JoystickButtonPressedEvent event(joystick_id, button_index);
            GetEventHandler()(event);
          } else {
            JoystickButtonReleasedEvent event(joystick_id, button_index);
            GetEventHandler()(event);
          }
        }
      }

      // axes
      for (int axis_index = 0; axis_index < axis_count; ++axis_index) {
        float previous_axis_value = previous_state.axes[axis_index];
        float current_axis_value = axis_values[axis_index];

        if (fabsf(current_axis_value - previous_axis_value) > 0.01f) {
          float adjusted_value =
              (fabsf(current_axis_value) < 0.10f) ? 0.0f : current_axis_value;
          JoystickAxisMovedEvent event(joystick_id, axis_index, adjusted_value);
          GetEventHandler()(event);
        }
      }

      // hats
      for (int hat_index = 0; hat_index < hat_count; ++hat_index) {
        unsigned char previous_hat = previous_state.hats[hat_index];
        unsigned char current_hat = hat_values[hat_index];

        if (previous_hat != current_hat) {
          JoystickHatChangedEvent event(joystick_id, hat_index, current_hat);
          GetEventHandler()(event);
        }
      }

      previous_state.axes.assign(axis_values, axis_values + axis_count);
      previous_state.buttons.assign(button_values,
                                    button_values + button_count);
      previous_state.hats.assign(hat_values, hat_values + hat_count);
    }
  }
}

bool GlfwAppWindow::IsShouldClose() {
  return glfwWindowShouldClose(handle_);
}

void GlfwAppWindow::CreateWindowSurface(VkInstance instance,
                                        VkSurfaceKHR* surface) {
  WIESEL_CHECK_VKRESULT(
      glfwCreateWindowSurface(instance, handle_, nullptr, surface));
}

void GlfwAppWindow::GetWindowFramebufferSize(WindowSize& size) {
  glfwGetFramebufferSize(handle_, &size.Width, &size.Height);
}

void GlfwAppWindow::SetCursorMode(CursorMode cursorMode) {
  cursor_mode_ = cursorMode;
  switch (cursorMode) {
    case CursorModeNormal: {
      glfwSetInputMode(handle_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      break;
    }
    case CursorModeRelative: {
      glfwSetInputMode(handle_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      glfwSetCursorPos(handle_, window_size_.Width / 2.0f,
                       window_size_.Height / 2.0f);
      break;
    }
  }
}

const char** GlfwAppWindow::GetRequiredInstanceExtensions(
    uint32_t* extensionsCount) {
  const char** glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(extensionsCount);
  return glfwExtensions;
}

void GlfwAppWindow::ImGuiInit() {
  ImGui_ImplGlfw_InitForVulkan(handle_, true);
}

void GlfwAppWindow::ImGuiNewFrame() {
  ImGui_ImplGlfw_NewFrame();
}

float_t Time::GetTime() {
  return glfwGetTime();
}

}  // namespace Wiesel