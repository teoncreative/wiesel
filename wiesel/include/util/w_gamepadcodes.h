//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Gamepad button and axis codes.
// Values match GLFW gamepad layout (which mirrors SDL GameController).
//

#pragma once

#include "w_pch.h"

namespace Wiesel {

using GamepadButton = int32_t;

enum : GamepadButton {
  GamepadButtonA = 0,
  GamepadButtonB = 1,
  GamepadButtonX = 2,
  GamepadButtonY = 3,
  GamepadButtonLB = 4,  // Left bumper
  GamepadButtonRB = 5,  // Right bumper
  GamepadButtonBack = 6,
  GamepadButtonStart = 7,
  GamepadButtonGuide = 8,
  GamepadButtonLeftStick = 9,
  GamepadButtonRightStick = 10,
  GamepadButtonDPadUp = 11,
  GamepadButtonDPadRight = 12,
  GamepadButtonDPadDown = 13,
  GamepadButtonDPadLeft = 14,
  GamepadButtonCount = 15,
};

using GamepadAxis = int32_t;

enum : GamepadAxis {
  GamepadAxisLeftX = 0,
  GamepadAxisLeftY = 1,
  GamepadAxisRightX = 2,
  GamepadAxisRightY = 3,
  GamepadAxisLeftTrigger = 4,
  GamepadAxisRightTrigger = 5,
  GamepadAxisCount = 6,
};

inline const char* GamepadButtonToString(GamepadButton button) {
  switch (button) {
    case GamepadButtonA:
      return "A";
    case GamepadButtonB:
      return "B";
    case GamepadButtonX:
      return "X";
    case GamepadButtonY:
      return "Y";
    case GamepadButtonLB:
      return "LB";
    case GamepadButtonRB:
      return "RB";
    case GamepadButtonBack:
      return "Back";
    case GamepadButtonStart:
      return "Start";
    case GamepadButtonGuide:
      return "Guide";
    case GamepadButtonLeftStick:
      return "LeftStick";
    case GamepadButtonRightStick:
      return "RightStick";
    case GamepadButtonDPadUp:
      return "DPadUp";
    case GamepadButtonDPadRight:
      return "DPadRight";
    case GamepadButtonDPadDown:
      return "DPadDown";
    case GamepadButtonDPadLeft:
      return "DPadLeft";
    default:
      return "Unknown";
  }
}

inline GamepadButton StringToGamepadButton(const std::string& str) {
  static const std::unordered_map<std::string, GamepadButton> map = {
      {"A", GamepadButtonA},
      {"B", GamepadButtonB},
      {"X", GamepadButtonX},
      {"Y", GamepadButtonY},
      {"LB", GamepadButtonLB},
      {"RB", GamepadButtonRB},
      {"Back", GamepadButtonBack},
      {"Start", GamepadButtonStart},
      {"Guide", GamepadButtonGuide},
      {"LeftStick", GamepadButtonLeftStick},
      {"RightStick", GamepadButtonRightStick},
      {"DPadUp", GamepadButtonDPadUp},
      {"DPadRight", GamepadButtonDPadRight},
      {"DPadDown", GamepadButtonDPadDown},
      {"DPadLeft", GamepadButtonDPadLeft},
  };
  auto it = map.find(str);
  return it != map.end() ? it->second : -1;
}

inline const char* GamepadAxisToString(GamepadAxis axis) {
  switch (axis) {
    case GamepadAxisLeftX:
      return "LeftStickX";
    case GamepadAxisLeftY:
      return "LeftStickY";
    case GamepadAxisRightX:
      return "RightStickX";
    case GamepadAxisRightY:
      return "RightStickY";
    case GamepadAxisLeftTrigger:
      return "LeftTrigger";
    case GamepadAxisRightTrigger:
      return "RightTrigger";
    default:
      return "Unknown";
  }
}

inline GamepadAxis StringToGamepadAxis(const std::string& str) {
  static const std::unordered_map<std::string, GamepadAxis> map = {
      {"LeftStickX", GamepadAxisLeftX},
      {"LeftStickY", GamepadAxisLeftY},
      {"RightStickX", GamepadAxisRightX},
      {"RightStickY", GamepadAxisRightY},
      {"LeftTrigger", GamepadAxisLeftTrigger},
      {"RightTrigger", GamepadAxisRightTrigger},
  };
  auto it = map.find(str);
  return it != map.end() ? it->second : -1;
}

inline const std::vector<GamepadButton>& GetAllGamepadButtons() {
  static const std::vector<GamepadButton> buttons = {
      GamepadButtonA,         GamepadButtonB,          GamepadButtonX,
      GamepadButtonY,         GamepadButtonLB,         GamepadButtonRB,
      GamepadButtonBack,      GamepadButtonStart,      GamepadButtonGuide,
      GamepadButtonLeftStick, GamepadButtonRightStick, GamepadButtonDPadUp,
      GamepadButtonDPadRight, GamepadButtonDPadDown,   GamepadButtonDPadLeft,
  };
  return buttons;
}

inline const std::vector<GamepadAxis>& GetAllGamepadAxes() {
  static const std::vector<GamepadAxis> axes = {
      GamepadAxisLeftX,  GamepadAxisLeftY,       GamepadAxisRightX,
      GamepadAxisRightY, GamepadAxisLeftTrigger, GamepadAxisRightTrigger,
  };
  return axes;
}

}  // namespace Wiesel