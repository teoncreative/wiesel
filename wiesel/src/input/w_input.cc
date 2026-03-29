
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "input/w_input.h"
#include "game/w_game_info.h"
#include "w_engine.h"

namespace Wiesel {

// --- Global state ---

// Keyboard
static std::map<KeyCode, KeyData> keys_;
static int mouse_x_ = 0;
static int mouse_y_ = 0;
static constexpr int kMaxMouseButtons = 8;
static KeyData mouse_buttons_[kMaxMouseButtons] = {};
static float mouse_axis_x_ = 0.0f;
static float mouse_axis_y_ = 0.0f;
static float mouse_axis_sens_x_ = 80.0f;
static float mouse_axis_sens_y_ = 80.0f;

// Gamepads
static constexpr int kMaxGamepads = 16;
static GamepadState gamepads_[kMaxGamepads];

// Players
static PlayerSlot players_[kMaxPlayers];

// Contexts (loaded from project settings)
static std::map<std::string, InputContext> contexts_;

static bool input_enabled_ = true;

// --- Helpers ---

static const InputContext* GetContext(const std::string& name) {
  auto it = contexts_.find(name);
  return it != contexts_.end() ? &it->second : nullptr;
}

static const InputContext* GetPlayerContext(int player) {
  if (player < 0 || player >= kMaxPlayers || !players_[player].active) {
    return nullptr;
  }
  return GetContext(players_[player].context_name);
}

static const InputAction* FindAction(const InputContext* ctx,
                                     const std::string& name) {
  if (!ctx) {
    return nullptr;
  }
  for (auto& a : ctx->actions) {
    if (a.name == name) {
      return &a;
    }
  }
  return nullptr;
}

static const InputAxisMapping* FindAxis(const InputContext* ctx,
                                        const std::string& name) {
  if (!ctx) {
    return nullptr;
  }
  for (auto& a : ctx->axes) {
    if (a.name == name) {
      return &a;
    }
  }
  return nullptr;
}

// Check if action is pressed for a given player slot
static bool IsActionActive(const PlayerSlot& slot, const InputAction* action,
                           bool (*check_key)(KeyCode),
                           bool (*check_btn)(int, GamepadButton)) {
  if (!action) {
    return false;
  }

  // Keyboard keys (only if player is not bound to a specific gamepad)
  if (slot.gamepad_index < 0) {
    for (auto code : action->keys) {
      if (check_key(code)) {
        return true;
      }
    }
  }

  // Gamepad buttons
  if (slot.gamepad_index >= 0) {
    for (auto btn : action->buttons) {
      if (check_btn(slot.gamepad_index, btn)) {
        return true;
      }
    }
  }

  return false;
}

// --- Event handlers ---

static bool OnKeyPressed(KeyPressedEvent& event) {
  keys_[event.GetKeyCode()].pressed = true;
  return false;
}

static bool OnKeyReleased(KeyReleasedEvent& event) {
  keys_[event.GetKeyCode()].pressed = false;
  return false;
}

static bool OnMouseMoved(MouseMovedEvent& event) {
  mouse_x_ = event.GetX();
  mouse_y_ = event.GetY();
  if (event.GetCursorMode() == CursorModeRelative) {
    mouse_axis_x_ += -mouse_axis_sens_x_ * event.GetDeltaX();
    mouse_axis_y_ += -mouse_axis_sens_y_ * event.GetDeltaY();
  }
  return false;
}

static bool OnMouseButtonPressed(MouseButtonPressedEvent& event) {
  int btn = static_cast<int>(event.GetMouseButton());
  if (btn >= 0 && btn < kMaxMouseButtons) {
    mouse_buttons_[btn].pressed = true;
  }
  return false;
}

static bool OnMouseButtonReleased(MouseButtonReleasedEvent& event) {
  int btn = static_cast<int>(event.GetMouseButton());
  if (btn >= 0 && btn < kMaxMouseButtons) {
    mouse_buttons_[btn].pressed = false;
  }
  return false;
}

static bool OnJoystickConnect(JoystickConnectedEvent& event) {
  int jid = event.GetJoystickId();
  if (jid >= 0 && jid < kMaxGamepads) {
    gamepads_[jid].connected = true;
    gamepads_[jid].jid = jid;
    gamepads_[jid].name = event.GetJoystickName();
    LOG_INFO("Gamepad connected: {} (slot {})", event.GetJoystickName(), jid);
  }
  return false;
}

static bool OnJoystickDisconnect(JoystickDisconnectedEvent& event) {
  int jid = event.GetJoystickId();
  if (jid >= 0 && jid < kMaxGamepads) {
    gamepads_[jid] = {};
    LOG_INFO("Gamepad disconnected: slot {}", jid);
  }
  return false;
}

static bool OnJoystickButtonPressed(JoystickButtonPressedEvent& event) {
  int jid = event.GetJoystickId();
  int btn = event.GetButton();
  if (jid >= 0 && jid < kMaxGamepads && btn >= 0 && btn < GamepadButtonCount) {
    gamepads_[jid].buttons[btn].pressed = true;
  }
  return false;
}

static bool OnJoystickButtonReleased(JoystickButtonReleasedEvent& event) {
  int jid = event.GetJoystickId();
  int btn = event.GetButton();
  if (jid >= 0 && jid < kMaxGamepads && btn >= 0 && btn < GamepadButtonCount) {
    gamepads_[jid].buttons[btn].pressed = false;
  }
  return false;
}

static bool OnJoystickAxisMoved(JoystickAxisMovedEvent& event) {
  int jid = event.GetJoystickId();
  int axis = event.GetAxis();
  if (jid >= 0 && jid < kMaxGamepads && axis >= 0 && axis < GamepadAxisCount) {
    gamepads_[jid].axes[axis] = event.GetValue();
  }
  return false;
}

// --- Raw key checks ---

static bool RawKeyPressed(KeyCode code) {
  return keys_[code].pressed;
}

static bool RawKeyDown(KeyCode code) {
  auto& kd = keys_[code];
  return kd.pressed && !kd.previous_pressed;
}

static bool RawKeyUp(KeyCode code) {
  auto& kd = keys_[code];
  return !kd.pressed && kd.previous_pressed;
}

static bool RawBtnPressed(int gp, GamepadButton btn) {
  if (gp < 0 || gp >= kMaxGamepads) {
    return false;
  }
  return gamepads_[gp].buttons[btn].pressed;
}

static bool RawBtnDown(int gp, GamepadButton btn) {
  if (gp < 0 || gp >= kMaxGamepads) {
    return false;
  }
  auto& bd = gamepads_[gp].buttons[btn];
  return bd.pressed && !bd.previous_pressed;
}

static bool RawBtnUp(int gp, GamepadButton btn) {
  if (gp < 0 || gp >= kMaxGamepads) {
    return false;
  }
  auto& bd = gamepads_[gp].buttons[btn];
  return !bd.pressed && bd.previous_pressed;
}

// --- InputManager ---

void InputManager::Init() {
  // Default: player 0 uses "keyboard" context with no gamepad
  players_[0].active = true;
  players_[0].context_name = "keyboard";
  players_[0].gamepad_index = -1;
}

void InputManager::LoadFromSettings(const InputSettings& settings) {
  contexts_ = settings.contexts;

  mouse_axis_sens_x_ = settings.mouse_sensitivity_x;
  mouse_axis_sens_y_ = settings.mouse_sensitivity_y;

  LOG_INFO("Input: loaded {} contexts", contexts_.size());
}

void InputManager::OnEvent(Event& event) {
  EventDispatcher dispatcher(event);

  dispatcher.Dispatch<KeyPressedEvent>(WIESEL_BIND_GLOBAL_FN(OnKeyPressed));
  dispatcher.Dispatch<KeyReleasedEvent>(WIESEL_BIND_GLOBAL_FN(OnKeyReleased));
  dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_GLOBAL_FN(OnMouseMoved));
  dispatcher.Dispatch<MouseButtonPressedEvent>(
      WIESEL_BIND_GLOBAL_FN(OnMouseButtonPressed));
  dispatcher.Dispatch<MouseButtonReleasedEvent>(
      WIESEL_BIND_GLOBAL_FN(OnMouseButtonReleased));
  dispatcher.Dispatch<JoystickConnectedEvent>(
      WIESEL_BIND_GLOBAL_FN(OnJoystickConnect));
  dispatcher.Dispatch<JoystickDisconnectedEvent>(
      WIESEL_BIND_GLOBAL_FN(OnJoystickDisconnect));
  dispatcher.Dispatch<JoystickButtonPressedEvent>(
      WIESEL_BIND_GLOBAL_FN(OnJoystickButtonPressed));
  dispatcher.Dispatch<JoystickButtonReleasedEvent>(
      WIESEL_BIND_GLOBAL_FN(OnJoystickButtonReleased));
  dispatcher.Dispatch<JoystickAxisMovedEvent>(
      WIESEL_BIND_GLOBAL_FN(OnJoystickAxisMoved));
}

void InputManager::Update() {
  // Reset per-frame mouse deltas
  mouse_axis_x_ = 0.0f;
  mouse_axis_y_ = 0.0f;

  // Update previous_pressed for keyboard
  for (auto& [code, data] : keys_) {
    data.previous_pressed = data.pressed;
  }
  // Update previous_pressed for mouse buttons
  for (int i = 0; i < kMaxMouseButtons; i++) {
    mouse_buttons_[i].previous_pressed = mouse_buttons_[i].pressed;
  }
  // Update previous_pressed for gamepad buttons
  for (int i = 0; i < kMaxGamepads; i++) {
    if (!gamepads_[i].connected) {
      continue;
    }
    for (int b = 0; b < GamepadButtonCount; b++) {
      gamepads_[i].buttons[b].previous_pressed =
          gamepads_[i].buttons[b].pressed;
    }
  }
}

// --- Single-player convenience (player 0) ---

bool InputManager::GetAction(const std::string& action) {
  return GetAction(0, action);
}

bool InputManager::GetActionDown(const std::string& action) {
  return GetActionDown(0, action);
}

bool InputManager::GetActionUp(const std::string& action) {
  return GetActionUp(0, action);
}

float InputManager::GetAxis(const std::string& axis_name) {
  return GetAxis(0, axis_name);
}

// --- Multi-player ---

bool InputManager::GetAction(int player, const std::string& action) {
  if (!input_enabled_) {
    return false;
  }
  if (player < 0 || player >= kMaxPlayers || !players_[player].active) {
    return false;
  }

  auto* ctx = GetPlayerContext(player);
  auto* act = FindAction(ctx, action);
  return IsActionActive(players_[player], act, RawKeyPressed, RawBtnPressed);
}

bool InputManager::GetActionDown(int player, const std::string& action) {
  if (!input_enabled_) {
    return false;
  }
  if (player < 0 || player >= kMaxPlayers || !players_[player].active) {
    return false;
  }

  auto* ctx = GetPlayerContext(player);
  auto* act = FindAction(ctx, action);
  return IsActionActive(players_[player], act, RawKeyDown, RawBtnDown);
}

bool InputManager::GetActionUp(int player, const std::string& action) {
  if (!input_enabled_) {
    return false;
  }
  if (player < 0 || player >= kMaxPlayers || !players_[player].active) {
    return false;
  }

  const InputContext* ctx = GetPlayerContext(player);
  const InputAction* act = FindAction(ctx, action);
  return IsActionActive(players_[player], act, RawKeyUp, RawBtnUp);
}

float InputManager::GetAxis(int player, const std::string& axis_name) {
  if (player < 0 || player >= kMaxPlayers || !players_[player].active) {
    return 0.0f;
  }

  // Mouse axes are always available for keyboard players
  PlayerSlot& slot = players_[player];
  if (slot.gamepad_index < 0) {
    if (axis_name == "Mouse X") {
      return mouse_axis_x_;
    }
    if (axis_name == "Mouse Y") {
      return mouse_axis_y_;
    }
  }

  const InputContext* ctx = GetPlayerContext(player);
  const InputAxisMapping* mapping = FindAxis(ctx, axis_name);
  if (!mapping) {
    return 0.0f;
  }

  // Gamepad analog axis (takes priority if player has a gamepad)
  if (slot.gamepad_index >= 0 && mapping->gamepad_axis >= 0) {
    float val = GetGamepadAxis(slot.gamepad_index, mapping->gamepad_axis);
    if (mapping->invert_axis) {
      val = -val;
    }
    if (std::abs(val) < mapping->dead_zone) {
      val = 0.0f;
    }
    return val;
  }

  // Digital keyboard axis
  if (slot.gamepad_index < 0) {
    bool pos = false, neg = false;
    for (int32_t code : mapping->positive_keys) {
      if (keys_[code].pressed) {
        pos = true;
        break;
      }
    }
    for (int32_t code : mapping->negative_keys) {
      if (keys_[code].pressed) {
        neg = true;
        break;
      }
    }
    if (pos && !neg) {
      return 1.0f;
    }
    if (neg && !pos) {
      return -1.0f;
    }
  }

  // Digital gamepad axis (DPad buttons as axis)
  // Not yet implemented - gamepad axes should cover this

  return 0.0f;
}

// --- Player management ---

void InputManager::AssignPlayer(int player, const std::string& context,
                                int gamepad_index) {
  if (player < 0 || player >= kMaxPlayers) {
    return;
  }
  players_[player].active = true;
  players_[player].context_name = context;
  players_[player].gamepad_index = gamepad_index;
  LOG_INFO("Player {} assigned to context '{}' (gamepad: {})", player, context,
           gamepad_index);
}

void InputManager::UnassignPlayer(int player) {
  if (player < 0 || player >= kMaxPlayers) {
    return;
  }
  players_[player] = {};
}

const PlayerSlot& InputManager::GetPlayerSlot(int player) {
  static PlayerSlot empty;
  if (player < 0 || player >= kMaxPlayers) {
    return empty;
  }
  return players_[player];
}

// --- Raw queries ---

bool InputManager::IsKeyPressed(KeyCode code) {
  if (!input_enabled_) {
    return false;
  }
  return keys_[code].pressed;
}

bool InputManager::IsGamepadButtonPressed(int gamepad_index,
                                          GamepadButton button) {
  if (gamepad_index < 0 || gamepad_index >= kMaxGamepads) {
    return false;
  }
  if (button < 0 || button >= GamepadButtonCount) {
    return false;
  }
  return gamepads_[gamepad_index].buttons[button].pressed;
}

float InputManager::GetGamepadAxis(int gamepad_index, GamepadAxis axis) {
  if (gamepad_index < 0 || gamepad_index >= kMaxGamepads) {
    return 0.0f;
  }
  if (axis < 0 || axis >= GamepadAxisCount) {
    return 0.0f;
  }
  return gamepads_[gamepad_index].axes[axis];
}

int InputManager::GetMouseX() {
  return mouse_x_;
}

int InputManager::GetMouseY() {
  return mouse_y_;
}

bool InputManager::IsMouseButtonPressed(MouseCode button) {
  int b = static_cast<int>(button);
  if (b < 0 || b >= kMaxMouseButtons) {
    return false;
  }
  return mouse_buttons_[b].pressed;
}

bool InputManager::IsMouseButtonDown(MouseCode button) {
  int b = static_cast<int>(button);
  if (b < 0 || b >= kMaxMouseButtons) {
    return false;
  }
  return mouse_buttons_[b].pressed && !mouse_buttons_[b].previous_pressed;
}

bool InputManager::IsMouseButtonUp(MouseCode button) {
  int b = static_cast<int>(button);
  if (b < 0 || b >= kMaxMouseButtons) {
    return false;
  }
  return !mouse_buttons_[b].pressed && mouse_buttons_[b].previous_pressed;
}

int InputManager::GetConnectedGamepadCount() {
  int count = 0;
  for (int i = 0; i < kMaxGamepads; i++) {
    if (gamepads_[i].connected) {
      count++;
    }
  }
  return count;
}

const GamepadState& InputManager::GetGamepadState(int index) {
  static GamepadState empty;
  if (index < 0 || index >= kMaxGamepads) {
    return empty;
  }
  return gamepads_[index];
}

InputMode InputManager::GetInputMode(int player) {
  if (player < 0 || player >= kMaxPlayers) {
    return kInputModeKeyboardAndMouse;
  }
  return players_[player].GetInputMode();
}

void InputManager::SetEnabled(bool enabled) {
  input_enabled_ = enabled;
}

bool InputManager::IsEnabled() {
  return input_enabled_;
}

}  // namespace Wiesel