
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

#include "w_pch.hpp"

namespace Wiesel {
	using KeyCode = int32_t; // Define KeyCode as int32_t
	enum : KeyCode {
		KeyUnknown = -1,

		/* Printable keys */
		KeySpace = 32,
		KeyApostrophe = 39, /* ' */
		KeyComma = 44, /* , */
		KeyMinus = 45, /* - */
		KeyPeriod = 46, /* . */
		KeySlash = 47, /* / */
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
		KeySemicolon = 59,  /* ; */
		KeyEqual = 61, /* = */
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
		KeyLeftBracket = 91,   /* [ */
		KeyBackslash = 92,  /* \ */
		KeyRightBracket = 93,  /* ] */
		KeyGraveAccent = 96, /* ` */
		KeyWorld1 = 161, /* non-US #1 */
		KeyWorld2 = 162, /* non-US #2 */

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
}
