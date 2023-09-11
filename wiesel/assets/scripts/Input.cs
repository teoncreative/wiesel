namespace WieselEngine
{
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
    }
}