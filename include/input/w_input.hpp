
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
    KeyData() { Pressed = false; }
    explicit KeyData(bool pressed) : Pressed(pressed){};

    bool Pressed;
  };

  enum InputMode {
    InputModeKeyboardAndMouse,
    InputModeGamepad
  };

  class InputManager {
  public:
    static bool GetKey(const std::string& key);
    static bool IsPressed(KeyCode keyCode);
    static float GetAxis(const std::string& axisName);
    static int GetMouseX() { return m_MouseX; }
    static int GetMouseY() { return m_MouseY; }

    static void Init();

  private:
    friend class Application;

    // todo maybe can be improved?
    static std::map<std::string, std::vector<KeyCode>> m_KeyboardMapping;
    static std::map<KeyCode, KeyData> m_Keys;
    static std::map<MouseCode, KeyData> m_MouseButtons;
    static std::map<std::string, float> m_Axis;
    static int m_MouseX;
    static int m_MouseY;
    static float m_MouseAxisSensX;
    static float m_MouseAxisSensY;
    static InputMode m_InputMode;
    static float m_MouseAxisLimitY;
  };

}