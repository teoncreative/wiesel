
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
#include "events/w_events.hpp"

namespace Wiesel {
struct KeyData {
  KeyData() : pressed(false), previous_pressed(false) {}
  explicit KeyData(bool pressed) : pressed(pressed), previous_pressed(false) {}

  bool pressed;
  bool previous_pressed;
};

enum InputMode { kInputModeKeyboardAndMouse, kInputModeGamepad };

class InputManager {
 public:
  static bool GetKey(const std::string& key);
  static bool GetKeyDown(const std::string& key);
  static bool GetKeyUp(const std::string& key);
  static bool IsPressed(KeyCode keyCode);
  static float GetAxis(const std::string& axisName);

  static int GetMouseX();
  static int GetMouseY();

  static void Init();
  static void Update();
  static void OnEvent(Event& event);

  // When disabled, GetKey/IsPressed/GetAxis return false/0 for scripts.
  // The underlying key state still updates so nothing is lost on re-enable.
  static void SetEnabled(bool enabled);
  static bool IsEnabled();

};

}  // namespace Wiesel