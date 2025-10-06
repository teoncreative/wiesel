
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

std::map<std::string, std::vector<KeyCode>> InputManager::keyboard_mapping_;
std::map<KeyCode, KeyData> InputManager::keys_;
std::map<MouseCode, KeyData> InputManager::mouse_buttons_;
std::map<std::string, float> InputManager::axis_;
int InputManager::mouse_x_ = 0;
int InputManager::mouse_y_ = 0;
float InputManager::mouse_axis_sens_x_ = 80.0f;
float InputManager::mouse_axis_sens_y_ = 80.0f;
InputMode InputManager::input_mode_ = kInputModeKeyboardAndMouse;
float InputManager::mouse_axis_limit_y_ = 75.0f;

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