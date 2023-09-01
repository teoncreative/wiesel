namespace WieselEngine
{
    public class Input
    {
        public static float GetAxis(string axis)
        {
            return EngineInternal.GetAxis(axis);
        }
        public static bool GetKey(string keyName)
        {
            return EngineInternal.GetKey(keyName);
        }
    }
}