
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

#include "events/w_appevents.h"
#include "events/w_events.h"
#include "events/w_keyevents.h"
#include "events/w_mouseevents.h"
#include "util/w_gamepadcodes.h"
#include "util/w_keycodes.h"
#include "util/w_mousecodes.h"

namespace Wiesel {
class MouseMovedEvent;

struct KeyData {
  KeyData() : pressed(false), previous_pressed(false) {}

  explicit KeyData(bool pressed) : pressed(pressed), previous_pressed(false) {}

  bool pressed;
  bool previous_pressed;
};

enum InputMode { kInputModeKeyboardAndMouse, kInputModeGamepad };

struct InputSettings;
struct InputContext;
struct InputAction;
struct InputAxisMapping;

// Max players supported
constexpr int kMaxPlayers = 4;

// Per-gamepad state
struct GamepadState {
  bool connected = false;
  int jid = -1;  // GLFW joystick ID
  std::string name;
  KeyData buttons[GamepadButtonCount] = {};
  float axes[GamepadAxisCount] = {};
};

// Per-player assignment
struct PlayerSlot {
  bool active = false;
  std::string context_name;  // which InputContext this player uses
  int gamepad_index = -1;    // index into gamepads_ array (-1 = keyboard/mouse)

  // Smoothed axis values for digital inputs (keyboard)
  std::unordered_map<std::string, float> smoothed_axes;

  InputMode GetInputMode() const {
    return gamepad_index >= 0 ? kInputModeGamepad : kInputModeKeyboardAndMouse;
  }
};

constexpr int kMaxMouseButtons = 8;
constexpr int kMaxGamepads = 16;

class InputManager {
 public:
  InputManager();

  // --- Single-player convenience (player 0) ---
  bool GetAction(const std::string& action);
  bool GetActionDown(const std::string& action);
  bool GetActionUp(const std::string& action);
  float GetAxis(const std::string& axis_name);

  // --- Multi-player ---
  bool GetAction(int player, const std::string& action);
  bool GetActionDown(int player, const std::string& action);
  bool GetActionUp(int player, const std::string& action);
  float GetAxis(int player, const std::string& axis_name);

  // --- Player management ---
  void AssignPlayer(int player, const std::string& context,
                    int gamepad_index = -1);
  void UnassignPlayer(int player);
  const PlayerSlot& GetPlayerSlot(int player);

  // --- Raw key queries (not context-aware) ---
  bool IsKeyPressed(KeyCode code);
  bool IsGamepadButtonPressed(int gamepad_index, GamepadButton button);
  float GetGamepadAxis(int gamepad_index, GamepadAxis axis);

  // --- Mouse ---
  int GetMouseX();
  int GetMouseY();
  bool IsMouseButtonPressed(MouseCode button);
  bool IsMouseButtonDown(MouseCode button);
  bool IsMouseButtonUp(MouseCode button);

  // --- Gamepad info ---
  int GetConnectedGamepadCount();
  const GamepadState& GetGamepadState(int index);

  // --- Input mode (per-player, derived from device assignment) ---
  InputMode GetInputMode(int player = 0);

  // --- System ---
  void Update();
  void OnEvent(Event& event);
  void LoadFromSettings(const InputSettings& settings);

  void SetEnabled(bool enabled);
  bool IsEnabled();

 private:
  // --- Helpers ---
  const InputContext* GetContext(const std::string& name);
  const InputContext* GetPlayerContext(int player);
  const InputAction* FindAction(const InputContext* ctx,
                                const std::string& name);
  const InputAxisMapping* FindAxis(const InputContext* ctx,
                                   const std::string& name);
  bool IsActionActive(const PlayerSlot& slot, const InputAction* action,
                      const std::function<bool(KeyCode)>& check_key,
                      const std::function<bool(int, GamepadButton)>& check_btn);

  // --- Raw key checks ---
  bool RawKeyPressed(KeyCode code);
  bool RawKeyDown(KeyCode code);
  bool RawKeyUp(KeyCode code);
  bool RawBtnPressed(int gp, GamepadButton btn);
  bool RawBtnDown(int gp, GamepadButton btn);
  bool RawBtnUp(int gp, GamepadButton btn);

  // --- Event handlers ---
  bool OnKeyPressed(KeyPressedEvent& event);
  bool OnKeyReleased(KeyReleasedEvent& event);
  bool OnMouseMoved(MouseMovedEvent& event);
  bool OnMouseButtonPressed(MouseButtonPressedEvent& event);
  bool OnMouseButtonReleased(MouseButtonReleasedEvent& event);
  bool OnJoystickConnect(JoystickConnectedEvent& event);
  bool OnJoystickDisconnect(JoystickDisconnectedEvent& event);
  bool OnJoystickButtonPressed(JoystickButtonPressedEvent& event);
  bool OnJoystickButtonReleased(JoystickButtonReleasedEvent& event);
  bool OnJoystickAxisMoved(JoystickAxisMovedEvent& event);

  // --- Data ---
  std::map<KeyCode, KeyData> keys_;
  int mouse_x_ = 0;
  int mouse_y_ = 0;
  KeyData mouse_buttons_[kMaxMouseButtons] = {};
  float mouse_axis_x_ = 0.0f;
  float mouse_axis_y_ = 0.0f;
  float mouse_axis_sens_x_ = 80.0f;
  float mouse_axis_sens_y_ = 80.0f;
  GamepadState gamepads_[kMaxGamepads] = {};
  PlayerSlot players_[kMaxPlayers] = {};
  std::map<std::string, InputContext> contexts_;
  bool input_enabled_ = true;
};

}  // namespace Wiesel