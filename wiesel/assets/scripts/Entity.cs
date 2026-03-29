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

        public bool HasTag(string tag)
        {
            return Internals.Entity_HasTag(scenePtr, entityId, tag);
        }

        public void AddTag(string tag)
        {
            Internals.Entity_AddTag(scenePtr, entityId, tag);
        }

        public void RemoveTag(string tag)
        {
            Internals.Entity_RemoveTag(scenePtr, entityId, tag);
        }

        public void AddComponent<T>()
        {
            Internals.Entity_AddComponent(scenePtr, entityId, typeof(T).Name);
        }

        public void RemoveComponent<T>()
        {
            Internals.Entity_RemoveComponent(scenePtr, entityId, typeof(T).Name);
        }

        public int ChildCount
        {
            get { return Internals.Entity_GetChildCount(scenePtr, entityId); }
        }

        public Entity GetChild(int index)
        {
            ulong childId = Internals.Entity_GetChild(scenePtr, entityId, index);
            if (childId == ulong.MaxValue) return null;
            return new Entity(scenePtr, childId);
        }

        public T GetChildComponent<T>(int index)
        {
            Entity child = GetChild(index);
            if (child == null) return default;
            return child.GetComponent<T>();
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
