
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "asset/w_asset_handle.h"
#include "events/w_events.h"
#include "util/w_mousecodes.h"
#include "w_pch.h"
#include "window/w_window.h"

namespace wiesel {

class AssetUnloadedEvent : public Event {
 public:
  AssetUnloadedEvent(AssetHandle handle) : handle_(handle) {}

  AssetHandle GetHandle() const { return handle_; }

  EVENT_CLASS_TYPE(AssetUnloaded)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
  AssetHandle handle_;
};

class WindowClosedEvent : public Event {
 public:
  WindowClosedEvent() {}

  EVENT_CLASS_TYPE(WindowClosed)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
};

class WindowResizedEvent : public Event {
 public:
  WindowResizedEvent(WindowSize window_size, float_t aspect_ratio)
      : window_size_(window_size), aspect_ratio_(aspect_ratio) {}

  WIESEL_GETTER_FN const WindowSize& window_size() { return window_size_; }

  WIESEL_GETTER_FN float aspect_ratio() const { return aspect_ratio_; }

  EVENT_CLASS_TYPE(WindowResized)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
  WindowSize window_size_;
  float_t aspect_ratio_;
};

class WindowMinimizedEvent : public Event {
 public:
  WindowMinimizedEvent() {}

  EVENT_CLASS_TYPE(WindowMinimized)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
};

class WindowRestoredEvent : public Event {
 public:
  WindowRestoredEvent() {}

  EVENT_CLASS_TYPE(WindowRestored)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
};

class WindowFocusGainedEvent : public Event {
 public:
  WindowFocusGainedEvent() {}

  EVENT_CLASS_TYPE(WindowFocusGained)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
};

class WindowFocusLostEvent : public Event {
 public:
  WindowFocusLostEvent() {}

  EVENT_CLASS_TYPE(WindowFocusLost)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
};

class AppRecreateSwapChainsEvent : public Event {
 public:
  AppRecreateSwapChainsEvent(WindowSize window_size, float_t aspect_ratio)
      : window_size_(window_size), aspect_ratio_(aspect_ratio) {}

  WIESEL_GETTER_FN const WindowSize& GetWindowSize() { return window_size_; }

  WIESEL_GETTER_FN float GetAspectRatio() const { return aspect_ratio_; }

  EVENT_CLASS_TYPE(AppRecreateSwapChains)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
  WindowSize window_size_;
  float_t aspect_ratio_;
};

class ScriptsReloadedEvent : public Event {
 public:
  ScriptsReloadedEvent() {}

  EVENT_CLASS_TYPE(ScriptsReloaded)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
};

class JoystickConnectedEvent : public Event {
 public:
  JoystickConnectedEvent(int jid, std::string name, bool is_gamepad)
      : jid_(jid), name_(name), is_gamepad_(is_gamepad) {}

  WIESEL_GETTER_FN int GetJoystickId() const { return jid_; }

  WIESEL_GETTER_FN const std::string& GetJoystickName() const { return name_; }

  WIESEL_GETTER_FN bool IsGamepad() const { return is_gamepad_; }

  EVENT_CLASS_TYPE(JoystickConnected)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
  int jid_;
  std::string name_;
  bool is_gamepad_;
};

class JoystickDisconnectedEvent : public Event {
 public:
  JoystickDisconnectedEvent(int jid) : jid_(jid) {}

  WIESEL_GETTER_FN int GetJoystickId() const { return jid_; }

  EVENT_CLASS_TYPE(JoystickDisconnected)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
  int jid_;
};

class JoystickButtonPressedEvent : public Event {
 public:
  JoystickButtonPressedEvent(int jid, int button)
      : jid_(jid), button_(button) {}

  WIESEL_GETTER_FN int GetJoystickId() const { return jid_; }

  WIESEL_GETTER_FN int GetButton() const { return button_; }

  EVENT_CLASS_TYPE(JoystickButtonPressed)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryInput)
 private:
  int jid_;
  int button_;
};

class JoystickButtonReleasedEvent : public Event {
 public:
  JoystickButtonReleasedEvent(int jid, int button)
      : jid_(jid), button_(button) {}

  WIESEL_GETTER_FN int GetJoystickId() const { return jid_; }

  WIESEL_GETTER_FN int GetButton() const { return button_; }

  EVENT_CLASS_TYPE(JoystickButtonReleased)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryInput)
 private:
  int jid_;
  int button_;
};

class JoystickAxisMovedEvent : public Event {
 public:
  JoystickAxisMovedEvent(int jid, int axis, float value)
      : jid_(jid), axis_(axis), value_(value) {}

  WIESEL_GETTER_FN int GetJoystickId() const { return jid_; }

  WIESEL_GETTER_FN int GetAxis() const { return axis_; }

  WIESEL_GETTER_FN float GetValue() const { return value_; }

  EVENT_CLASS_TYPE(JoystickAxisMoved)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryInput)
 private:
  int jid_;
  int axis_;
  float value_;
};

class JoystickHatChangedEvent : public Event {
 public:
  JoystickHatChangedEvent(int jid, int hat_index, int current_hat)
      : jid_(jid), hat_index_(hat_index), current_hat_(current_hat) {}

  WIESEL_GETTER_FN int GetJoystickId() const { return jid_; }

  WIESEL_GETTER_FN int GetHatIndex() const { return hat_index_; }

  WIESEL_GETTER_FN int GetCurrentHat() const { return current_hat_; }

  EVENT_CLASS_TYPE(JoystickHatChanged)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryInput)
 private:
  int jid_;
  int hat_index_;
  int current_hat_;
};

}  // namespace wiesel
