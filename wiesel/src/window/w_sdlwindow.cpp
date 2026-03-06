
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "window/w_sdlwindow.hpp"

#include <backends/imgui_impl_sdl3.h>

#include "events/w_appevents.hpp"
#include "events/w_keyevents.hpp"
#include "events/w_mouseevents.hpp"
#include "util/w_keycodes.hpp"

namespace Wiesel {

// Map SDL3 scancodes to engine KeyCode (GLFW-compatible values)
KeyCode SdlAppWindow::TranslateKeyCode(SDL_Scancode scancode) {
  // clang-format off
  switch (scancode) {
    case SDL_SCANCODE_SPACE:         return KeySpace;
    case SDL_SCANCODE_APOSTROPHE:    return KeyApostrophe;
    case SDL_SCANCODE_COMMA:         return KeyComma;
    case SDL_SCANCODE_MINUS:         return KeyMinus;
    case SDL_SCANCODE_PERIOD:        return KeyPeriod;
    case SDL_SCANCODE_SLASH:         return KeySlash;
    case SDL_SCANCODE_0:             return Key0;
    case SDL_SCANCODE_1:             return Key1;
    case SDL_SCANCODE_2:             return Key2;
    case SDL_SCANCODE_3:             return Key3;
    case SDL_SCANCODE_4:             return Key4;
    case SDL_SCANCODE_5:             return Key5;
    case SDL_SCANCODE_6:             return Key6;
    case SDL_SCANCODE_7:             return Key7;
    case SDL_SCANCODE_8:             return Key8;
    case SDL_SCANCODE_9:             return Key9;
    case SDL_SCANCODE_SEMICOLON:     return KeySemicolon;
    case SDL_SCANCODE_EQUALS:        return KeyEqual;
    case SDL_SCANCODE_A:             return KeyA;
    case SDL_SCANCODE_B:             return KeyB;
    case SDL_SCANCODE_C:             return KeyC;
    case SDL_SCANCODE_D:             return KeyD;
    case SDL_SCANCODE_E:             return KeyE;
    case SDL_SCANCODE_F:             return KeyF;
    case SDL_SCANCODE_G:             return KeyG;
    case SDL_SCANCODE_H:             return KeyH;
    case SDL_SCANCODE_I:             return KeyI;
    case SDL_SCANCODE_J:             return KeyJ;
    case SDL_SCANCODE_K:             return KeyK;
    case SDL_SCANCODE_L:             return KeyL;
    case SDL_SCANCODE_M:             return KeyM;
    case SDL_SCANCODE_N:             return KeyN;
    case SDL_SCANCODE_O:             return KeyO;
    case SDL_SCANCODE_P:             return KeyP;
    case SDL_SCANCODE_Q:             return KeyQ;
    case SDL_SCANCODE_R:             return KeyR;
    case SDL_SCANCODE_S:             return KeyS;
    case SDL_SCANCODE_T:             return KeyT;
    case SDL_SCANCODE_U:             return KeyU;
    case SDL_SCANCODE_V:             return KeyV;
    case SDL_SCANCODE_W:             return KeyW;
    case SDL_SCANCODE_X:             return KeyX;
    case SDL_SCANCODE_Y:             return KeyY;
    case SDL_SCANCODE_Z:             return KeyZ;
    case SDL_SCANCODE_LEFTBRACKET:   return KeyLeftBracket;
    case SDL_SCANCODE_BACKSLASH:     return KeyBackslash;
    case SDL_SCANCODE_RIGHTBRACKET:  return KeyRightBracket;
    case SDL_SCANCODE_GRAVE:         return KeyGraveAccent;
    case SDL_SCANCODE_ESCAPE:        return KeyEscape;
    case SDL_SCANCODE_RETURN:        return KeyEnter;
    case SDL_SCANCODE_TAB:           return KeyTab;
    case SDL_SCANCODE_BACKSPACE:     return KeyBackspace;
    case SDL_SCANCODE_INSERT:        return KeyInsert;
    case SDL_SCANCODE_DELETE:        return KeyDelete;
    case SDL_SCANCODE_RIGHT:         return KeyArrowRight;
    case SDL_SCANCODE_LEFT:          return KeyArrowLeft;
    case SDL_SCANCODE_DOWN:          return KeyArrowDown;
    case SDL_SCANCODE_UP:            return KeyArrowUp;
    case SDL_SCANCODE_PAGEUP:        return KeyPageUp;
    case SDL_SCANCODE_PAGEDOWN:      return KeyPageDown;
    case SDL_SCANCODE_HOME:          return KeyHome;
    case SDL_SCANCODE_END:           return KeyEnd;
    case SDL_SCANCODE_CAPSLOCK:      return KeyCapsLock;
    case SDL_SCANCODE_SCROLLLOCK:    return KeyScrollLock;
    case SDL_SCANCODE_NUMLOCKCLEAR:  return KeyNumLock;
    case SDL_SCANCODE_PRINTSCREEN:   return KeyPrintScreen;
    case SDL_SCANCODE_PAUSE:         return KeyPause;
    case SDL_SCANCODE_F1:            return KeyF1;
    case SDL_SCANCODE_F2:            return KeyF2;
    case SDL_SCANCODE_F3:            return KeyF3;
    case SDL_SCANCODE_F4:            return KeyF4;
    case SDL_SCANCODE_F5:            return KeyF5;
    case SDL_SCANCODE_F6:            return KeyF6;
    case SDL_SCANCODE_F7:            return KeyF7;
    case SDL_SCANCODE_F8:            return KeyF8;
    case SDL_SCANCODE_F9:            return KeyF9;
    case SDL_SCANCODE_F10:           return KeyF10;
    case SDL_SCANCODE_F11:           return KeyF11;
    case SDL_SCANCODE_F12:           return KeyF12;
    case SDL_SCANCODE_F13:           return KeyF13;
    case SDL_SCANCODE_F14:           return KeyF14;
    case SDL_SCANCODE_F15:           return KeyF15;
    case SDL_SCANCODE_F16:           return KeyF16;
    case SDL_SCANCODE_F17:           return KeyF17;
    case SDL_SCANCODE_F18:           return KeyF18;
    case SDL_SCANCODE_F19:           return KeyF19;
    case SDL_SCANCODE_F20:           return KeyF20;
    case SDL_SCANCODE_F21:           return KeyF21;
    case SDL_SCANCODE_F22:           return KeyF22;
    case SDL_SCANCODE_F23:           return KeyF23;
    case SDL_SCANCODE_F24:           return KeyF24;
    case SDL_SCANCODE_KP_0:          return KeyKeypad0;
    case SDL_SCANCODE_KP_1:          return KeyKeypad1;
    case SDL_SCANCODE_KP_2:          return KeyKeypad2;
    case SDL_SCANCODE_KP_3:          return KeyKeypad3;
    case SDL_SCANCODE_KP_4:          return KeyKeypad4;
    case SDL_SCANCODE_KP_5:          return KeyKeypad5;
    case SDL_SCANCODE_KP_6:          return KeyKeypad6;
    case SDL_SCANCODE_KP_7:          return KeyKeypad7;
    case SDL_SCANCODE_KP_8:          return KeyKeypad8;
    case SDL_SCANCODE_KP_9:          return KeyKeypad9;
    case SDL_SCANCODE_KP_PERIOD:     return KeyKeypadDecimal;
    case SDL_SCANCODE_KP_DIVIDE:     return KeyKeypadDivide;
    case SDL_SCANCODE_KP_MULTIPLY:   return KeyKeypadMultiply;
    case SDL_SCANCODE_KP_MINUS:      return KeyKeypadSubtract;
    case SDL_SCANCODE_KP_PLUS:       return KeyKeypadAdd;
    case SDL_SCANCODE_KP_ENTER:      return KeyKeypadEnter;
    case SDL_SCANCODE_KP_EQUALS:     return KeyKeypadEqual;
    case SDL_SCANCODE_LSHIFT:        return KeyLeftShift;
    case SDL_SCANCODE_LCTRL:         return KeyLeftControl;
    case SDL_SCANCODE_LALT:          return KeyLeftAlt;
    case SDL_SCANCODE_LGUI:          return KeyLeftSuper;
    case SDL_SCANCODE_RSHIFT:        return KeyRightShift;
    case SDL_SCANCODE_RCTRL:         return KeyRightControl;
    case SDL_SCANCODE_RALT:          return KeyRightAlt;
    case SDL_SCANCODE_RGUI:          return KeyRightSuper;
    case SDL_SCANCODE_MENU:          return KeyMenu;
    default:                         return KeyUnknown;
  }
  // clang-format on
}

SdlAppWindow::SdlAppWindow(const WindowProperties&& properties)
    : AppWindow(properties) {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
  LOG_DEBUG("SDL3 Vulkan Support: {}", SDL_Vulkan_LoadLibrary(nullptr));

  SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
  if (properties_.resizable) {
    flags |= SDL_WINDOW_RESIZABLE;
  }

  handle_ = SDL_CreateWindow(properties_.title.c_str(),
                              properties_.size.width, properties_.size.height,
                              flags);

  int w, h;
  SDL_GetWindowSizeInPixels(handle_, &w, &h);
  framebuffer_size_ = {w, h};
  SDL_GetWindowSize(handle_, &w, &h);
  window_size_ = {w, h};
  scale_.width = framebuffer_size_.width / static_cast<float>(window_size_.width);
  scale_.height = framebuffer_size_.height / static_cast<float>(window_size_.height);
}

SdlAppWindow::~SdlAppWindow() {
  LOG_DEBUG("Destroying SdlAppWindow");
  for (auto& [id, state] : gamepads_) {
    if (state.gamepad) {
      SDL_CloseGamepad(state.gamepad);
    }
  }
  SDL_DestroyWindow(handle_);
  SDL_Quit();
}

void SdlAppWindow::OnUpdate() {
  PROFILE_ZONE_SCOPED();

  // Coalesce mouse motion - accumulate dx/dy, dispatch once after loop
  double mouse_dx = 0.0, mouse_dy = 0.0;
  bool had_mouse_motion = false;

  SDL_Event e;
  {
    PROFILE_ZONE_SCOPED_N("SDL_PollEvent");
    while (SDL_PollEvent(&e)) {
      ImGui_ImplSDL3_ProcessEvent(&e);

      switch (e.type) {
        case SDL_EVENT_QUIT: {
          WindowCloseEvent event;
          GetEventHandler()(event);
          break;
        }

        case SDL_EVENT_WINDOW_RESIZED: {
          int w, h;
          SDL_GetWindowSizeInPixels(handle_, &w, &h);
          framebuffer_size_ = {w, h};
          SDL_GetWindowSize(handle_, &w, &h);
          window_size_ = {w, h};
          scale_.width = framebuffer_size_.width / static_cast<float>(window_size_.width);
          scale_.height = framebuffer_size_.height / static_cast<float>(window_size_.height);

          WindowResizeEvent event({w, h}, w / static_cast<float>(h));
          GetEventHandler()(event);
          break;
        }

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
          WindowCloseEvent event;
          GetEventHandler()(event);
          break;
        }

        case SDL_EVENT_KEY_DOWN: {
          KeyCode key = TranslateKeyCode(e.key.scancode);
          if (key != KeyUnknown) {
            KeyPressedEvent event(key, e.key.repeat);
            GetEventHandler()(event);
          }
          break;
        }

        case SDL_EVENT_KEY_UP: {
          KeyCode key = TranslateKeyCode(e.key.scancode);
          if (key != KeyUnknown) {
            KeyReleasedEvent event(key);
            GetEventHandler()(event);
          }
          break;
        }

        case SDL_EVENT_TEXT_INPUT: {
          // SDL3 gives us a UTF-8 string; send first codepoint
          const char* text = e.text.text;
          if (text && text[0]) {
            KeyTypedEvent event(static_cast<unsigned int>(text[0]));
            GetEventHandler()(event);
          }
          break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
          auto button = static_cast<MouseCode>(e.button.button - 1);  // SDL buttons are 1-based
          MouseButtonPressedEvent event(button);
          GetEventHandler()(event);
          break;
        }

        case SDL_EVENT_MOUSE_BUTTON_UP: {
          auto button = static_cast<MouseCode>(e.button.button - 1);
          MouseButtonReleasedEvent event(button);
          GetEventHandler()(event);
          break;
        }

        case SDL_EVENT_MOUSE_MOTION: {
          // Coalesce: accumulate delta, dispatch once after loop
          mouse_dx += e.motion.xrel;
          mouse_dy += e.motion.yrel;
          had_mouse_motion = true;
          break;
        }

        case SDL_EVENT_MOUSE_WHEEL: {
          MouseScrolledEvent event(e.wheel.x, e.wheel.y);
          GetEventHandler()(event);
          break;
        }

        case SDL_EVENT_GAMEPAD_ADDED: {
          SDL_JoystickID jid = e.gdevice.which;
          SDL_Gamepad* gp = SDL_OpenGamepad(jid);
          if (gp) {
            gamepads_[jid].gamepad = gp;
            JoystickConnectedEvent event(jid, SDL_GetGamepadName(gp), true);
            GetEventHandler()(event);
          }
          break;
        }

        case SDL_EVENT_GAMEPAD_REMOVED: {
          SDL_JoystickID jid = e.gdevice.which;
          auto it = gamepads_.find(jid);
          if (it != gamepads_.end()) {
            SDL_CloseGamepad(it->second.gamepad);
            gamepads_.erase(it);
          }
          JoystickDisconnectedEvent event(jid);
          GetEventHandler()(event);
          break;
        }

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
          JoystickButtonPressedEvent event(e.gbutton.which, e.gbutton.button);
          GetEventHandler()(event);
          break;
        }

        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
          JoystickButtonReleasedEvent event(e.gbutton.which, e.gbutton.button);
          GetEventHandler()(event);
          break;
        }

        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
          float value = e.gaxis.value / 32767.0f;
          float adjusted = (fabsf(value) < 0.15f) ? 0.0f : value;
          JoystickAxisMovedEvent event(e.gaxis.which, e.gaxis.axis, adjusted);
          GetEventHandler()(event);
          break;
        }

        default:
          break;
      }
    }
  }

  // Dispatch one coalesced mouse event per frame
  if (had_mouse_motion) {
    if (cursor_mode_ == CursorModeRelative) {
      MouseMovedEvent event(mouse_dx, mouse_dy, cursor_mode_);
      GetEventHandler()(event);
    } else {
      float mx, my;
      SDL_GetMouseState(&mx, &my);
      double x = static_cast<double>(mx) * scale_.width;
      double y = static_cast<double>(my) * scale_.height;
      MouseMovedEvent event(x, y, cursor_mode_);
      GetEventHandler()(event);
    }
  }
}

bool SdlAppWindow::IsShouldClose() {
  return should_close_;
}

void SdlAppWindow::CreateWindowSurface(VkInstance instance,
                                        VkSurfaceKHR* surface) {
  if (!SDL_Vulkan_CreateSurface(handle_, instance, nullptr, surface)) {
    LOG_ERROR("Failed to create Vulkan surface: {}", SDL_GetError());
  }
}

void SdlAppWindow::GetWindowFramebufferSize(WindowSize& size) {
  int w, h;
  SDL_GetWindowSizeInPixels(handle_, &w, &h);
  size = {w, h};
}

void SdlAppWindow::SetTitle(const std::string& title) {
  AppWindow::SetTitle(title);
  SDL_SetWindowTitle(handle_, title.c_str());
}

void SdlAppWindow::SetCursorMode(CursorMode cursor_mode) {
  cursor_mode_ = cursor_mode;
  switch (cursor_mode) {
    case CursorModeNormal: {
      SDL_SetWindowRelativeMouseMode(handle_, false);
      SDL_ShowCursor();
      break;
    }
    case CursorModeHidden: {
      SDL_SetWindowRelativeMouseMode(handle_, false);
      SDL_HideCursor();
      break;
    }
    case CursorModeRelative: {
      cursor_relative_first_ = true;
      cursor_delta_x_ = 0.0;
      cursor_delta_y_ = 0.0;
      SDL_SetWindowRelativeMouseMode(handle_, true);
      break;
    }
    case CursorModeUnlocked: {
      cursor_relative_first_ = true;
      cursor_delta_x_ = 0.0;
      cursor_delta_y_ = 0.0;
      SDL_SetWindowRelativeMouseMode(handle_, true);
      break;
    }
  }
}

void SdlAppWindow::WarpCursor(double x, double y) {
  SDL_WarpMouseInWindow(handle_, static_cast<float>(x), static_cast<float>(y));
}

void SdlAppWindow::GetCursorDelta(double& dx, double& dy) {
  dx = cursor_delta_x_;
  dy = cursor_delta_y_;
}

void SdlAppWindow::ResetCursorDelta() {
  cursor_delta_x_ = 0.0;
  cursor_delta_y_ = 0.0;
}

const char** SdlAppWindow::GetRequiredInstanceExtensions(
    uint32_t* extensionsCount) {
  Uint32 count = 0;
  const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
  *extensionsCount = count;
  return const_cast<const char**>(extensions);
}

void SdlAppWindow::ImGuiInit() {
  ImGui_ImplSDL3_InitForVulkan(handle_);
}

void SdlAppWindow::ImGuiNewFrame() {
  ImGui_ImplSDL3_NewFrame();
}

float_t Time::GetTime() {
  return static_cast<float_t>(SDL_GetTicks()) / 1000.0;
}

}  // namespace Wiesel