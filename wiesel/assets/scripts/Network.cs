using System;

namespace WieselEngine
{
    public enum NetworkRole
    {
        None = 0,
        Server = 1,
        Client = 2,
        ListenServer = 3
    }

    public static class Network
    {
        public static bool StartServer(string ip = "0.0.0.0", int port = 25000)
        {
            return Internals.Network_StartServer(ip, port);
        }

        public static void StopServer()
        {
            Internals.Network_StopServer();
        }

        public static bool ConnectToServer(string ip = "127.0.0.1", int port = 25000)
        {
            return Internals.Network_ConnectToServer(ip, port);
        }

        public static void Disconnect()
        {
            Internals.Network_Disconnect();
        }

        public static bool IsServer
        {
            get { return Internals.Network_IsServer(); }
        }

        public static bool IsClient
        {
            get { return Internals.Network_IsClient(); }
        }

        public static bool IsConnected
        {
            get { return Internals.Network_IsConnected(); }
        }

        public static NetworkRole Role
        {
            get { return (NetworkRole)Internals.Network_GetRole(); }
        }

        public static int TickRate
        {
            get { return Internals.Network_GetTickRate(); }
            set { Internals.Network_SetTickRate(value); }
        }

        public static ulong LocalSessionId
        {
            get { return Internals.Network_GetLocalSessionId(); }
        }
    }
}
