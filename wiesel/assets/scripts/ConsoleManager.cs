using System;
using System.Collections.Generic;

namespace WieselEngine
{
    // Mirrors C++ wiesel::ParamType.
    public enum ParamType
    {
        Int = 0,
        Float = 1,
        Bool = 2,
        String = 3,
        Vec2 = 4,
        Vec3 = 5,
        Vec4 = 6,
    }

    // Parameter descriptor. Use the Params helpers below to build one.
    public struct Param
    {
        public string Name;
        public ParamType Type;
        public bool Optional;
        public string DefaultTokens;
    }

    // Factories + Make helper so call sites can write
    //   Params.Make(Params.Vec3("pos"), Params.String("name", "Entity"))
    public static class Params
    {
        public static Param Int(string name) => new Param { Name = name, Type = ParamType.Int };
        public static Param Int(string name, int defaultValue) => new Param { Name = name, Type = ParamType.Int, Optional = true, DefaultTokens = defaultValue.ToString(System.Globalization.CultureInfo.InvariantCulture) };
        public static Param Float(string name) => new Param { Name = name, Type = ParamType.Float };
        public static Param Float(string name, float defaultValue) => new Param { Name = name, Type = ParamType.Float, Optional = true, DefaultTokens = defaultValue.ToString(System.Globalization.CultureInfo.InvariantCulture) };
        public static Param Bool(string name) => new Param { Name = name, Type = ParamType.Bool };
        public static Param Bool(string name, bool defaultValue) => new Param { Name = name, Type = ParamType.Bool, Optional = true, DefaultTokens = defaultValue ? "true" : "false" };
        public static Param String(string name) => new Param { Name = name, Type = ParamType.String };
        public static Param String(string name, string defaultValue) => new Param { Name = name, Type = ParamType.String, Optional = true, DefaultTokens = defaultValue };
        public static Param Vec2(string name) => new Param { Name = name, Type = ParamType.Vec2 };
        public static Param Vec2(string name, Vector2f defaultValue) => new Param { Name = name, Type = ParamType.Vec2, Optional = true, DefaultTokens = FormatVec(defaultValue.X, defaultValue.Y) };
        public static Param Vec3(string name) => new Param { Name = name, Type = ParamType.Vec3 };
        public static Param Vec3(string name, Vector3f defaultValue) => new Param { Name = name, Type = ParamType.Vec3, Optional = true, DefaultTokens = FormatVec(defaultValue.X, defaultValue.Y, defaultValue.Z) };
        public static Param Vec4(string name) => new Param { Name = name, Type = ParamType.Vec4 };
        public static Param Vec4(string name, Vector4f defaultValue) => new Param { Name = name, Type = ParamType.Vec4, Optional = true, DefaultTokens = FormatVec(defaultValue.X, defaultValue.Y, defaultValue.Z, defaultValue.W) };

        public static Param[] Make(params Param[] parameters) => parameters;

        private static string FormatVec(params float[] xs)
        {
            var parts = new string[xs.Length];
            for (int i = 0; i < xs.Length; i++)
                parts[i] = xs[i].ToString(System.Globalization.CultureInfo.InvariantCulture);
            return string.Join(" ", parts);
        }
    }

    // Passed to command callbacks. Holds a native pointer that's only
    // valid for the duration of the callback; don't cache it.
    public sealed class CommandContext
    {
        private readonly IntPtr native_;

        public CommandContext(IntPtr native) { native_ = native; }

        public int Int(string name) => Internals.CommandContext_Int(native_, name);
        public float Float(string name) => Internals.CommandContext_Float(native_, name);
        public bool Bool(string name) => Internals.CommandContext_Bool(native_, name);
        public string String(string name) => Internals.CommandContext_String(native_, name);
        public Vector2f Vec2(string name)
        {
            Internals.CommandContext_Vec2(native_, name, out float x, out float y);
            return new Vector2f(x, y);
        }
        public Vector3f Vec3(string name)
        {
            Internals.CommandContext_Vec3(native_, name, out float x, out float y, out float z);
            return new Vector3f(x, y, z);
        }
        public Vector4f Vec4(string name)
        {
            Internals.CommandContext_Vec4(native_, name, out float x, out float y, out float z, out float w);
            return new Vector4f(x, y, z, w);
        }
        public bool Has(string name) => Internals.CommandContext_Has(native_, name);
    }

    public static class ConsoleManager
    {
        public static void Register(string name, string description,
                                    Param[] parameters,
                                    Action<CommandContext> callback)
        {
            int count = parameters != null ? parameters.Length : 0;
            var names = new string[count];
            var types = new int[count];
            var optionals = new bool[count];
            var defaults = new string[count];
            for (int i = 0; i < count; i++)
            {
                names[i] = parameters[i].Name;
                types[i] = (int)parameters[i].Type;
                optionals[i] = parameters[i].Optional;
                defaults[i] = parameters[i].DefaultTokens ?? string.Empty;
            }
            // Wrap the native pointer into a typed CommandContext so user
            // code never sees the raw IntPtr.
            Action<IntPtr> wrapped = ptr => callback(new CommandContext(ptr));
            Internals.ConsoleManager_RegisterCommand(name, description, names, types, optionals, defaults, wrapped);
        }

        public static void Register(string name, string description,
                                    Action<CommandContext> callback)
        {
            Register(name, description, Array.Empty<Param>(), callback);
        }

        public static void Unregister(string name)
        {
            Internals.ConsoleManager_UnregisterCommand(name);
        }

        public static void Execute(string commandLine)
        {
            Internals.ConsoleManager_Execute(commandLine);
        }
    }
}
