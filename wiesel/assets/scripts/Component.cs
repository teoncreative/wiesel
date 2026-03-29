namespace WieselEngine
{
    public abstract class Component
    {
        protected ulong scenePtr;
        protected ulong entityId;

        public Entity Entity
        {
            get { return new Entity(scenePtr, entityId); }
        }

        public T GetComponent<T>()
        {
            return (T)Internals.Behavior_GetComponent(scenePtr, entityId, typeof(T).Name);
        }

        public bool HasComponent<T>()
        {
            return Internals.Behavior_HasComponent(scenePtr, entityId, typeof(T).Name);
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
    }
}