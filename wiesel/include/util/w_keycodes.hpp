
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

#include "w_pch.hpp"

namespace Wiesel {
using KeyCode = int32_t;  // Define KeyCode as int32_t

enum : KeyCode {
  KeyUnknown = -1,

  /* Printable keys */
  KeySpace = 32,
  KeyApostrophe = 39, /* ' */
  KeyComma = 44,      /* , */
  KeyMinus = 45,      /* - */
  KeyPeriod = 46,     /* . */
  KeySlash = 47,      /* / */
  Key0 = 48,
  Key1 = 49,
  Key2 = 50,
  Key3 = 51,
  Key4 = 52,
  Key5 = 53,
  Key6 = 54,
  Key7 = 55,
  Key8 = 56,
  Key9 = 57,
  KeySemicolon = 59, /* ; */
  KeyEqual = 61,     /* = */
  KeyA = 65,
  KeyB = 66,
  KeyC = 67,
  KeyD = 68,
  KeyE = 69,
  KeyF = 70,
  KeyG = 71,
  KeyH = 72,
  KeyI = 73,
  KeyJ = 74,
  KeyK = 75,
  KeyL = 76,
  KeyM = 77,
  KeyN = 78,
  KeyO = 79,
  KeyP = 80,
  KeyQ = 81,
  KeyR = 82,
  KeyS = 83,
  KeyT = 84,
  KeyU = 85,
  KeyV = 86,
  KeyW = 87,
  KeyX = 88,
  KeyY = 89,
  KeyZ = 90,
  KeyLeftBracket = 91,  /* [ */
  KeyBackslash = 92,    /* \ */
  KeyRightBracket = 93, /* ] */
  KeyGraveAccent = 96,  /* ` */
  KeyWorld1 = 161,      /* non-US #1 */
  KeyWorld2 = 162,      /* non-US #2 */

  /* Function keys */
  KeyEscape = 256,
  KeyEnter = 257,
  KeyTab = 258,
  KeyBackspace = 259,
  KeyInsert = 260,
  KeyDelete = 261,
  KeyArrowRight = 262,
  KeyArrowLeft = 263,
  KeyArrowDown = 264,
  KeyArrowUp = 265,
  KeyPageUp = 266,
  KeyPageDown = 267,
  KeyHome = 268,
  KeyEnd = 269,
  KeyCapsLock = 280,
  KeyScrollLock = 281,
  KeyNumLock = 282,
  KeyPrintScreen = 283,
  KeyPause = 284,
  KeyF1 = 290,
  KeyF2 = 291,
  KeyF3 = 292,
  KeyF4 = 293,
  KeyF5 = 294,
  KeyF6 = 295,
  KeyF7 = 296,
  KeyF8 = 297,
  KeyF9 = 298,
  KeyF10 = 299,
  KeyF11 = 300,
  KeyF12 = 301,
  KeyF13 = 302,
  KeyF14 = 303,
  KeyF15 = 304,
  KeyF16 = 305,
  KeyF17 = 306,
  KeyF18 = 307,
  KeyF19 = 308,
  KeyF20 = 309,
  KeyF21 = 310,
  KeyF22 = 311,
  KeyF23 = 312,
  KeyF24 = 313,
  KeyF25 = 314,
  KeyKeypad0 = 320,
  KeyKeypad1 = 321,
  KeyKeypad2 = 322,
  KeyKeypad3 = 323,
  KeyKeypad4 = 324,
  KeyKeypad5 = 325,
  KeyKeypad6 = 326,
  KeyKeypad7 = 327,
  KeyKeypad8 = 328,
  KeyKeypad9 = 329,
  KeyKeypadDecimal = 330,
  KeyKeypadDivide = 331,
  KeyKeypadMultiply = 332,
  KeyKeypadSubtract = 333,
  KeyKeypadAdd = 334,
  KeyKeypadEnter = 335,
  KeyKeypadEqual = 336,
  KeyLeftShift = 340,
  KeyLeftControl = 341,
  KeyLeftAlt = 342,
  KeyLeftSuper = 343,
  KeyRightShift = 344,
  KeyRightControl = 345,
  KeyRightAlt = 346,
  KeyRightSuper = 347,
  KeyMenu = 348
};

inline const char* KeyCodeToString(KeyCode code) {
  switch (code) {
    case KeySpace:
      return "Space";
    case KeyApostrophe:
      return "'";
    case KeyComma:
      return ",";
    case KeyMinus:
      return "-";
    case KeyPeriod:
      return ".";
    case KeySlash:
      return "/";
    case Key0:
      return "0";
    case Key1:
      return "1";
    case Key2:
      return "2";
    case Key3:
      return "3";
    case Key4:
      return "4";
    case Key5:
      return "5";
    case Key6:
      return "6";
    case Key7:
      return "7";
    case Key8:
      return "8";
    case Key9:
      return "9";
    case KeySemicolon:
      return ";";
    case KeyEqual:
      return "=";
    case KeyA:
      return "A";
    case KeyB:
      return "B";
    case KeyC:
      return "C";
    case KeyD:
      return "D";
    case KeyE:
      return "E";
    case KeyF:
      return "F";
    case KeyG:
      return "G";
    case KeyH:
      return "H";
    case KeyI:
      return "I";
    case KeyJ:
      return "J";
    case KeyK:
      return "K";
    case KeyL:
      return "L";
    case KeyM:
      return "M";
    case KeyN:
      return "N";
    case KeyO:
      return "O";
    case KeyP:
      return "P";
    case KeyQ:
      return "Q";
    case KeyR:
      return "R";
    case KeyS:
      return "S";
    case KeyT:
      return "T";
    case KeyU:
      return "U";
    case KeyV:
      return "V";
    case KeyW:
      return "W";
    case KeyX:
      return "X";
    case KeyY:
      return "Y";
    case KeyZ:
      return "Z";
    case KeyLeftBracket:
      return "[";
    case KeyBackslash:
      return "\\";
    case KeyRightBracket:
      return "]";
    case KeyGraveAccent:
      return "`";
    case KeyEscape:
      return "Escape";
    case KeyEnter:
      return "Enter";
    case KeyTab:
      return "Tab";
    case KeyBackspace:
      return "Backspace";
    case KeyInsert:
      return "Insert";
    case KeyDelete:
      return "Delete";
    case KeyArrowRight:
      return "Right";
    case KeyArrowLeft:
      return "Left";
    case KeyArrowDown:
      return "Down";
    case KeyArrowUp:
      return "Up";
    case KeyPageUp:
      return "Page Up";
    case KeyPageDown:
      return "Page Down";
    case KeyHome:
      return "Home";
    case KeyEnd:
      return "End";
    case KeyCapsLock:
      return "Caps Lock";
    case KeyScrollLock:
      return "Scroll Lock";
    case KeyNumLock:
      return "Num Lock";
    case KeyPrintScreen:
      return "Print Screen";
    case KeyPause:
      return "Pause";
    case KeyF1:
      return "F1";
    case KeyF2:
      return "F2";
    case KeyF3:
      return "F3";
    case KeyF4:
      return "F4";
    case KeyF5:
      return "F5";
    case KeyF6:
      return "F6";
    case KeyF7:
      return "F7";
    case KeyF8:
      return "F8";
    case KeyF9:
      return "F9";
    case KeyF10:
      return "F10";
    case KeyF11:
      return "F11";
    case KeyF12:
      return "F12";
    case KeyF13:
      return "F13";
    case KeyF14:
      return "F14";
    case KeyF15:
      return "F15";
    case KeyF16:
      return "F16";
    case KeyF17:
      return "F17";
    case KeyF18:
      return "F18";
    case KeyF19:
      return "F19";
    case KeyF20:
      return "F20";
    case KeyF21:
      return "F21";
    case KeyF22:
      return "F22";
    case KeyF23:
      return "F23";
    case KeyF24:
      return "F24";
    case KeyF25:
      return "F25";
    case KeyKeypad0:
      return "KP 0";
    case KeyKeypad1:
      return "KP 1";
    case KeyKeypad2:
      return "KP 2";
    case KeyKeypad3:
      return "KP 3";
    case KeyKeypad4:
      return "KP 4";
    case KeyKeypad5:
      return "KP 5";
    case KeyKeypad6:
      return "KP 6";
    case KeyKeypad7:
      return "KP 7";
    case KeyKeypad8:
      return "KP 8";
    case KeyKeypad9:
      return "KP 9";
    case KeyKeypadDecimal:
      return "KP .";
    case KeyKeypadDivide:
      return "KP /";
    case KeyKeypadMultiply:
      return "KP *";
    case KeyKeypadSubtract:
      return "KP -";
    case KeyKeypadAdd:
      return "KP +";
    case KeyKeypadEnter:
      return "KP Enter";
    case KeyKeypadEqual:
      return "KP =";
    case KeyLeftShift:
      return "Left Shift";
    case KeyLeftControl:
      return "Left Ctrl";
    case KeyLeftAlt:
      return "Left Alt";
    case KeyLeftSuper:
      return "Left Super";
    case KeyRightShift:
      return "Right Shift";
    case KeyRightControl:
      return "Right Ctrl";
    case KeyRightAlt:
      return "Right Alt";
    case KeyRightSuper:
      return "Right Super";
    case KeyMenu:
      return "Menu";
    default:
      return "Unknown";
  }
}

inline KeyCode StringToKeyCode(const std::string& str) {
  static const std::unordered_map<std::string, KeyCode> map = {
      {"Space", KeySpace},
      {"'", KeyApostrophe},
      {",", KeyComma},
      {"-", KeyMinus},
      {".", KeyPeriod},
      {"/", KeySlash},
      {"0", Key0},
      {"1", Key1},
      {"2", Key2},
      {"3", Key3},
      {"4", Key4},
      {"5", Key5},
      {"6", Key6},
      {"7", Key7},
      {"8", Key8},
      {"9", Key9},
      {";", KeySemicolon},
      {"=", KeyEqual},
      {"A", KeyA},
      {"B", KeyB},
      {"C", KeyC},
      {"D", KeyD},
      {"E", KeyE},
      {"F", KeyF},
      {"G", KeyG},
      {"H", KeyH},
      {"I", KeyI},
      {"J", KeyJ},
      {"K", KeyK},
      {"L", KeyL},
      {"M", KeyM},
      {"N", KeyN},
      {"O", KeyO},
      {"P", KeyP},
      {"Q", KeyQ},
      {"R", KeyR},
      {"S", KeyS},
      {"T", KeyT},
      {"U", KeyU},
      {"V", KeyV},
      {"W", KeyW},
      {"X", KeyX},
      {"Y", KeyY},
      {"Z", KeyZ},
      {"[", KeyLeftBracket},
      {"\\", KeyBackslash},
      {"]", KeyRightBracket},
      {"`", KeyGraveAccent},
      {"Escape", KeyEscape},
      {"Enter", KeyEnter},
      {"Tab", KeyTab},
      {"Backspace", KeyBackspace},
      {"Insert", KeyInsert},
      {"Delete", KeyDelete},
      {"Right", KeyArrowRight},
      {"Left", KeyArrowLeft},
      {"Down", KeyArrowDown},
      {"Up", KeyArrowUp},
      {"Page Up", KeyPageUp},
      {"Page Down", KeyPageDown},
      {"Home", KeyHome},
      {"End", KeyEnd},
      {"Caps Lock", KeyCapsLock},
      {"Scroll Lock", KeyScrollLock},
      {"Num Lock", KeyNumLock},
      {"Print Screen", KeyPrintScreen},
      {"Pause", KeyPause},
      {"F1", KeyF1},
      {"F2", KeyF2},
      {"F3", KeyF3},
      {"F4", KeyF4},
      {"F5", KeyF5},
      {"F6", KeyF6},
      {"F7", KeyF7},
      {"F8", KeyF8},
      {"F9", KeyF9},
      {"F10", KeyF10},
      {"F11", KeyF11},
      {"F12", KeyF12},
      {"F13", KeyF13},
      {"F14", KeyF14},
      {"F15", KeyF15},
      {"F16", KeyF16},
      {"F17", KeyF17},
      {"F18", KeyF18},
      {"F19", KeyF19},
      {"F20", KeyF20},
      {"F21", KeyF21},
      {"F22", KeyF22},
      {"F23", KeyF23},
      {"F24", KeyF24},
      {"F25", KeyF25},
      {"KP 0", KeyKeypad0},
      {"KP 1", KeyKeypad1},
      {"KP 2", KeyKeypad2},
      {"KP 3", KeyKeypad3},
      {"KP 4", KeyKeypad4},
      {"KP 5", KeyKeypad5},
      {"KP 6", KeyKeypad6},
      {"KP 7", KeyKeypad7},
      {"KP 8", KeyKeypad8},
      {"KP 9", KeyKeypad9},
      {"KP .", KeyKeypadDecimal},
      {"KP /", KeyKeypadDivide},
      {"KP *", KeyKeypadMultiply},
      {"KP -", KeyKeypadSubtract},
      {"KP +", KeyKeypadAdd},
      {"KP Enter", KeyKeypadEnter},
      {"KP =", KeyKeypadEqual},
      {"Left Shift", KeyLeftShift},
      {"Left Ctrl", KeyLeftControl},
      {"Left Alt", KeyLeftAlt},
      {"Left Super", KeyLeftSuper},
      {"Right Shift", KeyRightShift},
      {"Right Ctrl", KeyRightControl},
      {"Right Alt", KeyRightAlt},
      {"Right Super", KeyRightSuper},
      {"Menu", KeyMenu},
  };
  auto it = map.find(str);
  return it != map.end() ? it->second : KeyUnknown;
}

// All common key codes for dropdown selection
inline const std::vector<KeyCode>& GetAllKeyCodes() {
  static const std::vector<KeyCode> codes = {
      KeySpace,
      KeyApostrophe,
      KeyComma,
      KeyMinus,
      KeyPeriod,
      KeySlash,
      Key0,
      Key1,
      Key2,
      Key3,
      Key4,
      Key5,
      Key6,
      Key7,
      Key8,
      Key9,
      KeySemicolon,
      KeyEqual,
      KeyA,
      KeyB,
      KeyC,
      KeyD,
      KeyE,
      KeyF,
      KeyG,
      KeyH,
      KeyI,
      KeyJ,
      KeyK,
      KeyL,
      KeyM,
      KeyN,
      KeyO,
      KeyP,
      KeyQ,
      KeyR,
      KeyS,
      KeyT,
      KeyU,
      KeyV,
      KeyW,
      KeyX,
      KeyY,
      KeyZ,
      KeyLeftBracket,
      KeyBackslash,
      KeyRightBracket,
      KeyGraveAccent,
      KeyEscape,
      KeyEnter,
      KeyTab,
      KeyBackspace,
      KeyInsert,
      KeyDelete,
      KeyArrowRight,
      KeyArrowLeft,
      KeyArrowDown,
      KeyArrowUp,
      KeyPageUp,
      KeyPageDown,
      KeyHome,
      KeyEnd,
      KeyCapsLock,
      KeyScrollLock,
      KeyNumLock,
      KeyPrintScreen,
      KeyPause,
      KeyF1,
      KeyF2,
      KeyF3,
      KeyF4,
      KeyF5,
      KeyF6,
      KeyF7,
      KeyF8,
      KeyF9,
      KeyF10,
      KeyF11,
      KeyF12,
      KeyKeypad0,
      KeyKeypad1,
      KeyKeypad2,
      KeyKeypad3,
      KeyKeypad4,
      KeyKeypad5,
      KeyKeypad6,
      KeyKeypad7,
      KeyKeypad8,
      KeyKeypad9,
      KeyKeypadDecimal,
      KeyKeypadDivide,
      KeyKeypadMultiply,
      KeyKeypadSubtract,
      KeyKeypadAdd,
      KeyKeypadEnter,
      KeyKeypadEqual,
      KeyLeftShift,
      KeyLeftControl,
      KeyLeftAlt,
      KeyLeftSuper,
      KeyRightShift,
      KeyRightControl,
      KeyRightAlt,
      KeyRightSuper,
      KeyMenu,
  };
  return codes;
}

}  // namespace Wiesel
