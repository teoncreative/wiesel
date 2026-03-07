using System;

namespace WieselEngine
{
    public static class Console
    {
        public static void RegisterCommand(string name, string description, Action<string[]> callback)
        {
            Internals.Console_RegisterCommand(name, description, callback);
        }

        public static void UnregisterCommand(string name)
        {
            Internals.Console_UnregisterCommand(name);
        }

        public static void Execute(string commandLine)
        {
            Internals.Console_Execute(commandLine);
        }

        public static void Log(string message)
        {
            Internals.Console_LogInfo(message);
        }

        public static void LogWarning(string message)
        {
            Internals.Console_LogWarning(message);
        }

        public static void LogError(string message)
        {
            Internals.Console_LogError(message);
        }
    }
}