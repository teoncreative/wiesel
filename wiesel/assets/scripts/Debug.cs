namespace WieselEngine
{
    public static class Debug
    {
        public static void Log(string message)
        {
            Internals.Debug_Log(message);
        }

        public static void LogWarning(string message)
        {
            Internals.Debug_LogWarning(message);
        }

        public static void LogError(string message)
        {
            Internals.Debug_LogError(message);
        }
    }
}
