namespace WieselEngine
{
    public enum NetworkAuthority : int
    {
        None = 0,
        Server = 1,
        Client = 2
    }

    public class NetworkIdentityComponent : Component
    {
        public NetworkIdentityComponent(Entity entity) : base(entity) { }

        public uint NetId
        {
            get { return Internals.NetworkIdentity_GetNetId(entity.ScenePtr, entity.Id); }
        }

        public NetworkAuthority Authority
        {
            get { return (NetworkAuthority)Internals.NetworkIdentity_GetAuthority(entity.ScenePtr, entity.Id); }
            set { Internals.NetworkIdentity_SetAuthority(entity.ScenePtr, entity.Id, (int)value); }
        }

        public ulong OwnerSessionId
        {
            get { return Internals.NetworkIdentity_GetOwnerSessionId(entity.ScenePtr, entity.Id); }
            set { Internals.NetworkIdentity_SetOwnerSessionId(entity.ScenePtr, entity.Id, value); }
        }

        public bool IsOwnedByUs()
        {
            ulong localSession = Network.LocalSessionId;
            if (Network.IsServer && !Network.IsClient)
            {
                return OwnerSessionId == 0;
            }
            return OwnerSessionId == localSession;
        }
    }
}
