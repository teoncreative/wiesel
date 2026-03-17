
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
#include "util/w_gamepadcodes.hpp"
#include "events/w_events.hpp"

namespace Wiesel {
struct KeyData {
  KeyData() : pressed(false), previous_pressed(false) {}
  explicit KeyData(bool pressed) : pressed(pressed), previous_pressed(false) {}

  bool pressed;
  bool previous_pressed;
};

enum InputMode { kInputModeKeyboardAndMouse, kInputModeGamepad };

struct InputSettings;
struct InputContext;

// Max players supported
constexpr int kMaxPlayers = 4;

// Per-gamepad state
struct GamepadState {
  bool connected = false;
  int jid = -1;                                  // GLFW joystick ID
  std::string name;
  KeyData buttons[GamepadButtonCount] = {};
  float axes[GamepadAxisCount] = {};
};

// Per-player assignment
struct PlayerSlot {
  bool active = false;
  std::string context_name;   // which InputContext this player uses
  int gamepad_index = -1;     // index into gamepads_ array (-1 = keyboard/mouse)
};

class InputManager {
 public:
  // --- Single-player convenience (player 0) ---
  static bool GetAction(const std::string& action);
  static bool GetActionDown(const std::string& action);
  static bool GetActionUp(const std::string& action);
  static float GetAxis(const std::string& axis_name);

  // --- Multi-player ---
  static bool GetAction(int player, const std::string& action);
  static bool GetActionDown(int player, const std::string& action);
  static bool GetActionUp(int player, const std::string& action);
  static float GetAxis(int player, const std::string& axis_name);

  // --- Player management ---
  static void AssignPlayer(int player, const std::string& context, int gamepad_index = -1);
  static void UnassignPlayer(int player);
  static const PlayerSlot& GetPlayerSlot(int player);

  // --- Raw key queries (not context-aware) ---
  static bool IsKeyPressed(KeyCode code);
  static bool IsGamepadButtonPressed(int gamepad_index, GamepadButton button);
  static float GetGamepadAxis(int gamepad_index, GamepadAxis axis);

  // --- Mouse ---
  static int GetMouseX();
  static int GetMouseY();

  // --- Gamepad info ---
  static int GetConnectedGamepadCount();
  static const GamepadState& GetGamepadState(int index);

  // --- System ---
  static void Init();
  static void Update();
  static void OnEvent(Event& event);
  static void LoadFromSettings(const InputSettings& settings);

  static void SetEnabled(bool enabled);
  static bool IsEnabled();
};

}  // namespace Wiesel