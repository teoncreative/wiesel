
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "util/w_keycodes.hpp"
#include "util/w_mousecodes.hpp"

namespace Wiesel {
struct KeyData {
  KeyData() { pressed = false; }
  explicit KeyData(bool pressed) : pressed(pressed) { }

  bool pressed;
};

enum InputMode { kInputModeKeyboardAndMouse, kInputModeGamepad };

class InputManager {
 public:
  static bool GetKey(const std::string& key);
  static bool IsPressed(KeyCode keyCode);
  static float GetAxis(const std::string& axisName);

  static int mouse_x() { return mouse_x_; }
  static int mouse_y() { return mouse_y_; }

  static void Init();

 private:
  friend class Application;

  // todo maybe can be improved?
  static std::map<std::string, std::vector<KeyCode>> keyboard_mapping_;
  static std::map<KeyCode, KeyData> keys_;
  static std::map<MouseCode, KeyData> mouse_buttons_;
  static std::map<std::string, float> axis_;
  static int mouse_x_;
  static int mouse_y_;
  static float mouse_axis_sens_x_;
  static float mouse_axis_sens_y_;
  static InputMode input_mode_;
  static float mouse_axis_limit_y_;
};

}  // namespace Wiesel