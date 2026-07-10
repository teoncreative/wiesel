using System;

namespace WieselEngine
{
    [AttributeUsage(AttributeTargets.Method)]
    public class ServerRpcAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.Method)]
    public class ClientRpcAttribute : Attribute
    {
    }
}
