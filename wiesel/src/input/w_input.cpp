
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "input/w_input.hpp"
#include "w_engine.hpp"

namespace Wiesel {

std::map<std::string, std::vector<KeyCode>> keyboard_mapping_;
std::map<KeyCode, KeyData> keys_;
std::map<MouseCode, KeyData> mouse_buttons_;
std::map<std::string, float> axis_;
int mouse_x_ = 0;
int mouse_y_ = 0;
float mouse_axis_sens_x_ = 80.0f;
float mouse_axis_sens_y_ = 80.0f;
InputMode input_mode_ = kInputModeKeyboardAndMouse;
float mouse_axis_limit_y_ = 75.0f;

static void UpdateKeyboardAxis() {
  bool right = InputManager::GetKey("Right");
  bool left = InputManager::GetKey("Left");
  bool up = InputManager::GetKey("Up");
  bool down = InputManager::GetKey("Down");

  if (right && !left) {
    axis_["Horizontal"] = 1;
  } else if (!right && left) {
    axis_["Horizontal"] = -1;
  } else {
    axis_["Horizontal"] = 0;
  }

  if (up && !down) {
    axis_["Vertical"] = 1;
  } else if (!up && down) {
    axis_["Vertical"] = -1;
  } else {
    axis_["Vertical"] = 0;
  }
}

static bool OnKeyPressed(KeyPressedEvent& event) {
  input_mode_ = kInputModeKeyboardAndMouse;
  keys_[event.GetKeyCode()].pressed = true;
  UpdateKeyboardAxis();
  return false;
}

static bool OnKeyReleased(KeyReleasedEvent& event) {
  keys_[event.GetKeyCode()].pressed = false;
  UpdateKeyboardAxis();
  return false;
}

static bool OnMouseMoved(MouseMovedEvent& event) {
  input_mode_ = kInputModeKeyboardAndMouse;
  mouse_x_ = event.GetX();
  mouse_y_ = event.GetY();
  // todo mouse delta raw
  if (event.GetCursorMode() == CursorModeRelative) {
    axis_["Mouse X"] += mouse_axis_sens_x_ * event.GetX();
    axis_["Mouse Y"] += mouse_axis_sens_y_ * event.GetY();
    axis_["Mouse Y"] = std::clamp(
        axis_["Mouse Y"], -mouse_axis_limit_y_,
        mouse_axis_limit_y_);
  }
  return false;
}

static bool OnJoystickConnect(JoystickConnectedEvent& event) {
  return false;
}

static bool OnJoystickDisconnect(JoystickDisconnectedEvent& event) {
  return false;
}

static bool OnJoystickButtonPressed(JoystickButtonPressedEvent& event) {
  return false;
}

static bool OnJoystickButtonReleased(JoystickButtonReleasedEvent& event) {
  return false;
}

static bool OnJoystickButtonAxisMoved(JoystickAxisMovedEvent& event) {
  return false;
}

void InputManager::Init() {
  keyboard_mapping_["Up"] = {KeyArrowUp, KeyW};
  keyboard_mapping_["Down"] = {KeyArrowDown, KeyS};
  keyboard_mapping_["Left"] = {KeyArrowLeft, KeyA};
  keyboard_mapping_["Right"] = {KeyArrowRight, KeyD};
  keyboard_mapping_["Jump"] = {KeySpace};
  keyboard_mapping_["Enter"] = {KeyEnter};
  keyboard_mapping_["Left Shift"] = {KeyLeftShift};
  keyboard_mapping_["Right Shift"] = {KeyRightShift};
  keyboard_mapping_["Shift"] = {KeyLeftShift, KeyRightShift};
  keyboard_mapping_["Left Control"] = {KeyLeftControl};
  keyboard_mapping_["Right Control"] = {KeyRightControl};
  keyboard_mapping_["Control"] = {KeyLeftControl, KeyRightControl};
  keyboard_mapping_["Tab"] = {KeyTab};
  keyboard_mapping_["Return"] = {KeyBackspace};
}

void InputManager::OnEvent(Event& event) {
  EventDispatcher dispatcher(event);

  dispatcher.Dispatch<KeyPressedEvent>(WIESEL_BIND_GLOBAL_FN(OnKeyPressed));
  dispatcher.Dispatch<KeyReleasedEvent>(WIESEL_BIND_GLOBAL_FN(OnKeyReleased));
  dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_GLOBAL_FN(OnMouseMoved));
  dispatcher.Dispatch<JoystickConnectedEvent>(WIESEL_BIND_GLOBAL_FN(OnJoystickConnect));
  dispatcher.Dispatch<JoystickDisconnectedEvent>(WIESEL_BIND_GLOBAL_FN(OnJoystickDisconnect));
  dispatcher.Dispatch<JoystickButtonPressedEvent>(WIESEL_BIND_GLOBAL_FN(OnJoystickButtonPressed));
  dispatcher.Dispatch<JoystickButtonReleasedEvent>(WIESEL_BIND_GLOBAL_FN(OnJoystickButtonReleased));
  dispatcher.Dispatch<JoystickAxisMovedEvent>(WIESEL_BIND_GLOBAL_FN(OnJoystickButtonAxisMoved));
}

bool InputManager::GetKey(const std::string& key) {
  for (const auto& code : keyboard_mapping_[key]) {
    if (keys_[code].pressed) {
      return true;
    }
  }
  return false;
}

bool InputManager::IsPressed(KeyCode code) {
  return keys_[code].pressed;
}

float InputManager::GetAxis(const std::string& axisName) {
  return axis_[axisName];
}


}  // namespace Wiesel