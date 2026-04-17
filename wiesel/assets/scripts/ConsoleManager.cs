using System;

namespace WieselEngine
{
    public static class ConsoleManager
    {
        public static void RegisterCommand(string name, string description, Action<string[]> callback)
        {
            Internals.ConsoleManager_RegisterCommand(name, description, callback);
        }

        public static void UnregisterCommand(string name)
        {
            Internals.ConsoleManager_UnregisterCommand(name);
        }

        public static void Execute(string commandLine)
        {
            Internals.ConsoleManager_Execute(commandLine);
        }
    }
}
