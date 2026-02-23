namespace WieselEngine
{
    public class Entity
    {
        private ulong scenePtr;
        private ulong entityId;

        public ulong Id { get { return entityId; } }

        public Entity(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        public T GetComponent<T>()
        {
            return (T)Internals.Behavior_GetComponent(scenePtr, entityId, typeof(T).Name);
        }

        public bool HasComponent<T>()
        {
            return Internals.Behavior_HasComponent(scenePtr, entityId, typeof(T).Name);
        }

        public override bool Equals(object obj)
        {
            if (obj is Entity other)
                return entityId == other.entityId;
            return false;
        }

        public override int GetHashCode()
        {
            return entityId.GetHashCode();
        }

        public static bool operator ==(Entity a, Entity b)
        {
            if (ReferenceEquals(a, b)) return true;
            if (ReferenceEquals(a, null) || ReferenceEquals(b, null)) return false;
            return a.entityId == b.entityId;
        }

        public static bool operator !=(Entity a, Entity b)
        {
            return !(a == b);
        }
    }
}
