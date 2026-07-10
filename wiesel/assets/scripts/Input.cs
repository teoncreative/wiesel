namespace WieselEngine
{
    public enum CursorMode : ushort {
        Normal = 0,
        Hidden = 1,
        Relative = 2,
        Unlocked = 3
    }

    public enum KeyCode : int {
      Unknown = -1,

      /* Printable keys */
      Space = 32,
      Apostrophe = 39, /* ' */
      Comma = 44,      /* , */
      Minus = 45,      /* - */
      Period = 46,     /* . */
      Slash = 47,      /* / */
      Number0 = 48,
      Number1 = 49,
      Number2 = 50,
      Number3 = 51,
      Number4 = 52,
      Number5 = 53,
      Number6 = 54,
      Number7 = 55,
      Number8 = 56,
      Number9 = 57,
      Semicolon = 59, /* ; */
      Equal = 61,     /* = */
      A = 65,
      B = 66,
      C = 67,
      D = 68,
      E = 69,
      F = 70,
      G = 71,
      H = 72,
      I = 73,
      J = 74,
      K = 75,
      L = 76,
      M = 77,
      N = 78,
      O = 79,
      P = 80,
      Q = 81,
      R = 82,
      S = 83,
      T = 84,
      U = 85,
      V = 86,
      W = 87,
      X = 88,
      Y = 89,
      Z = 90,
      LeftBracket = 91,  /* [ */
      Backslash = 92,    /* \ */
      RightBracket = 93, /* ] */
      GraveAccent = 96,  /* ` */
      World1 = 161,      /* non-US #1 */
      World2 = 162,      /* non-US #2 */

      /* Function keys */
      Escape = 256,
      Enter = 257,
      Tab = 258,
      Backspace = 259,
      Insert = 260,
      Delete = 261,
      ArrowRight = 262,
      ArrowLeft = 263,
      ArrowDown = 264,
      ArrowUp = 265,
      PageUp = 266,
      PageDown = 267,
      Home = 268,
      End = 269,
      CapsLock = 280,
      ScrollLock = 281,
      NumLock = 282,
      PrintScreen = 283,
      Pause = 284,
      F1 = 290,
      F2 = 291,
      F3 = 292,
      F4 = 293,
      F5 = 294,
      F6 = 295,
      F7 = 296,
      F8 = 297,
      F9 = 298,
      F10 = 299,
      F11 = 300,
      F12 = 301,
      F13 = 302,
      F14 = 303,
      F15 = 304,
      F16 = 305,
      F17 = 306,
      F18 = 307,
      F19 = 308,
      F20 = 309,
      F21 = 310,
      F22 = 311,
      F23 = 312,
      F24 = 313,
      F25 = 314,
      Keypad0 = 320,
      Keypad1 = 321,
      Keypad2 = 322,
      Keypad3 = 323,
      Keypad4 = 324,
      Keypad5 = 325,
      Keypad6 = 326,
      Keypad7 = 327,
      Keypad8 = 328,
      Keypad9 = 329,
      KeypadDecimal = 330,
      KeypadDivide = 331,
      KeypadMultiply = 332,
      KeypadSubtract = 333,
      KeypadAdd = 334,
      KeypadEnter = 335,
      KeypadEqual = 336,
      LeftShift = 340,
      LeftControl = 341,
      LeftAlt = 342,
      LeftSuper = 343,
      RightShift = 344,
      RightControl = 345,
      RightAlt = 346,
      RightSuper = 347,
      Menu = 348
    };

    public enum GamepadButton : int
    {
        A = 0,
        B = 1,
        X = 2,
        Y = 3,
        LB = 4,
        RB = 5,
        Back = 6,
        Start = 7,
        Guide = 8,
        LeftStick = 9,
        RightStick = 10,
        DPadUp = 11,
        DPadRight = 12,
        DPadDown = 13,
        DPadLeft = 14
    }

    public enum GamepadAxis : int
    {
        LeftX = 0,
        LeftY = 1,
        RightX = 2,
        RightY = 3,
        LeftTrigger = 4,
        RightTrigger = 5
    }

    public class Input
    {
        public static float GetAxis(string axis)
        {
            return Internals.Input_GetAxis(axis);
        }

        public static bool GetKey(string keyName)
        {
            return Internals.Input_GetKey(keyName);
        }

        public static bool GetKeyDown(string keyName)
        {
            return Internals.Input_GetKeyDown(keyName);
        }

        public static bool GetKeyUp(string keyName)
        {
            return Internals.Input_GetKeyUp(keyName);
        }

        public static void SetCursorMode(CursorMode mode)
        {
            Internals.Input_SetCursorMode((ushort) mode);
        }

        public static CursorMode GetCursorMode()
        {
            return (CursorMode) Internals.Input_GetCursorMode();
        }

        public static int MouseX
        {
            get { return Internals.Input_GetMouseX(); }
        }

        public static int MouseY
        {
            get { return Internals.Input_GetMouseY(); }
        }

        public static Vector2f MousePosition
        {
            get { return new Vector2f(Internals.Input_GetMouseX(), Internals.Input_GetMouseY()); }
        }

        public static bool GetMouseButton(int button)
        {
            return Internals.Input_GetMouseButton(button);
        }

        public static bool GetMouseButtonDown(int button)
        {
            return Internals.Input_GetMouseButtonDown(button);
        }

        public static bool GetMouseButtonUp(int button)
        {
            return Internals.Input_GetMouseButtonUp(button);
        }

        public static bool GetGamepadButton(int gamepadIndex, GamepadButton button)
        {
            return Internals.Input_GetGamepadButton(gamepadIndex, (int)button);
        }

        public static float GetGamepadAxis(int gamepadIndex, GamepadAxis axis)
        {
            return Internals.Input_GetGamepadAxis(gamepadIndex, (int)axis);
        }

        public static int ConnectedGamepadCount
        {
            get { return Internals.Input_GetConnectedGamepadCount(); }
        }
    }
}