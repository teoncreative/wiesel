using System.Runtime.CompilerServices;

namespace WieselEngine
{
    /// <summary>
    /// Static API for controlling the custom cursor.
    /// </summary>
    public static class Cursor
    {
        /// <summary>Set the cursor to a named state from the active cursor set (e.g. "default", "pointer", "crosshair").</summary>
        public static void SetState(string state)
        {
            Internals.Cursor_SetState(state);
        }

        /// <summary>Get the current cursor state name.</summary>
        public static string GetState()
        {
            return Internals.Cursor_GetState();
        }
    }
}
