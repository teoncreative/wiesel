using System.Runtime.CompilerServices;

namespace WieselEngine
{
    public class Log
    {
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static void Info(string message);

    }
}