using System.Runtime.CompilerServices;

namespace WieselEngine
{
    public class EngineInternal
    {
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static void LogInfo(string message);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static float GetAxis(string axis);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static bool GetKey(string key);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static object GetComponent(MonoBehavior behavior, string name);

    }
}