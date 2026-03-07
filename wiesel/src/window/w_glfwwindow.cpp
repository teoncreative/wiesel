
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

  handle_ = glfwCreateWindow(properties_.size.width, properties_.size.height,
                             properties_.title.c_str(), nullptr, nullptr);
  glfwSetWindowUserPointer(handle_, this);

  glfwGetFramebufferSize(handle_, &framebuffer_size_.width,
                         &framebuffer_size_.height);
  glfwGetWindowSize(handle_, &window_size_.width, &window_size_.height);
  scale_.width = framebuffer_size_.width / static_cast<float>(window_size_.width);
  scale_.height = framebuffer_size_.height / static_cast<float>(window_size_.height);

  glfwSetWindowSizeCallback(
      handle_, [](GLFWwindow* window, int width, int height) {
        GlfwAppWindow& app_window =
            *static_cast<GlfwAppWindow*>(glfwGetWindowUserPointer(window));

        glfwGetFramebufferSize(app_window.handle_,
                               &app_window.framebuffer_size_.width,
                               &app_window.framebuffer_size_.height);
        glfwGetWindowSize(app_window.handle_, &app_window.window_size_.width,
                          &app_window.window_size_.height);
        app_window.scale_.width = app_window.framebuffer_size_.width /
                                 (float)app_window.window_size_.width;
        app_window.scale_.height = app_window.framebuffer_size_.height /
                                  (float)app_window.window_size_.height;

        WindowResizeEvent event({width, height}, width / (float)height);
        app_window.GetEventHandler()(event);
      });

  glfwSetWindowCloseCallback(handle_, [](GLFWwindow* window) {
    GlfwAppWindow& appWindow =
        *static_cast<GlfwAppWindow*>(glfwGetWindowUserPointer(window));

    WindowCloseEvent event;
    appWindow.GetEventHandler()(event);
  });

  glfwSetKeyCallback(handle_, [](GLFWwindow* window, int key, int scancode,
                                 int action, int mods) {
    GlfwAppWindow& app_window =
        *static_cast<GlfwAppWindow*>(glfwGetWindowUserPointer(window));

    switch (action) {
      case GLFW_PRESS: {
        KeyPressedEvent event(key, false);
        app_window.GetEventHandler()(event);
        break;
      }
      case GLFW_RELEASE: {
        KeyReleasedEvent event(key);
        app_window.GetEventHandler()(event);
        break;
      }
      case GLFW_REPEAT: {
        KeyPressedEvent event(key, true);
        app_window.GetEventHandler()(event);
        break;
      }
    }
  });
  glfwSetCharCallback(handle_, [](GLFWwindow* window, unsigned int keycode) {
    GlfwAppWindow& app_window =
        *static_cast<GlfwAppWindow*>(glfwGetWindowUserPointer(window));

    KeyTypedEvent event(keycode);
    app_window.GetEventHandler()(event);
  });

  glfwSetMouseButtonCallback(
      handle_, [](GLFWwindow* window, int button, int action, int mods) {
        GlfwAppWindow& app_window =
            *static_cast<GlfwAppWindow*>(glfwGetWindowUserPointer(window));

        switch (action) {
          case GLFW_PRESS: {
            MouseButtonPressedEvent event(static_cast<MouseCode>(button));
            app_window.GetEventHandler()(event);
            break;
          }
          case GLFW_RELEASE: {
            MouseButtonReleasedEvent event(static_cast<MouseCode>(button));
            app_window.GetEventHandler()(event);
            break;
          }
          default:
            break;
        }
      });

  glfwSetScrollCallback(
      handle_, [](GLFWwindow* window, double xOffset, double yOffset) {
        GlfwAppWindow& app_window =
            *static_cast<GlfwAppWindow*>(glfwGetWindowUserPointer(window));

        MouseScrolledEvent event((float)xOffset, (float)yOffset);
        app_window.GetEventHandler()(event);
      });
}

GlfwAppWindow::~GlfwAppWindow() {
  LOG_DEBUG("Destroying GlfwAppWindow");
  glfwDestroyWindow(handle_);
}

void GlfwAppWindow::OnUpdate() {
  PROFILE_ZONE_SCOPED();
  {
    PROFILE_ZONE_SCOPED_N("glfwPollEvents");
    glfwPollEvents();
  }

  // Poll cursor position once per frame
  double cur_x, cur_y;
  glfwGetCursorPos(handle_, &cur_x, &cur_y);

  if (cursor_relative_first_) {
    prev_cursor_x_ = cur_x;
    prev_cursor_y_ = cur_y;
    cursor_relative_first_ = false;
  }

  double dx = cur_x - prev_cursor_x_;
  double dy = cur_y - prev_cursor_y_;
  prev_cursor_x_ = cur_x;
  prev_cursor_y_ = cur_y;

  if (dx != 0.0 || dy != 0.0) {
    double norm_dx = dx / window_size_.width;
    double norm_dy = dy / window_size_.width;
    if (cursor_mode_ == CursorModeRelative) {
      MouseMovedEvent event(cur_x, cur_y, norm_dx, norm_dy, cursor_mode_);
      GetEventHandler()(event);
    } else {
      double x = cur_x * scale_.width;
      double y = cur_y * scale_.height;
      MouseMovedEvent event(x, y, norm_dx, norm_dy, cursor_mode_);
      GetEventHandler()(event);
    }
  }

  if (first_frame_) [[unlikely]] {
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
  glfwGetFramebufferSize(handle_, &size.width, &size.height);
}

void GlfwAppWindow::SetTitle(const std::string& title) {
  AppWindow::SetTitle(title);
  glfwSetWindowTitle(handle_, title.c_str());
}

void GlfwAppWindow::SetCursorMode(CursorMode cursor_mode) {
  cursor_mode_ = cursor_mode;
  switch (cursor_mode) {
    case CursorModeNormal: {
      glfwSetInputMode(handle_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      break;
    }
    case CursorModeHidden: {
      glfwSetInputMode(handle_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
      break;
    }
    case CursorModeRelative:
    case CursorModeUnlocked: {
      cursor_relative_first_ = true;
      cursor_delta_x_ = 0.0f;
      cursor_delta_y_ = 0.0f;
      glfwSetInputMode(handle_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      break;
    }
  }
  glfwSetInputMode(handle_, GLFW_RAW_MOUSE_MOTION, cursor_mode == CursorModeRelative);
}

void GlfwAppWindow::WarpCursor(double x, double y) {
  glfwSetCursorPos(handle_, x, y);
}

void GlfwAppWindow::GetCursorDelta(double& dx, double& dy) {
  dx = cursor_delta_x_;
  dy = cursor_delta_y_;
}

void GlfwAppWindow::ResetCursorDelta() {
  cursor_delta_x_ = 0.0f;
  cursor_delta_y_ = 0.0f;
}

const char** GlfwAppWindow::GetRequiredInstanceExtensions(
    uint32_t* extensionsCount) {
  const char** glfw_extensions;
  glfw_extensions = glfwGetRequiredInstanceExtensions(extensionsCount);
  return glfw_extensions;
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