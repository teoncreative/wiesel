namespace WieselEngine
{
    public abstract class Component
    {
        protected Entity entity;

        public Entity Entity
        {
            get { return entity; }
        }

        public T GetComponent<T>()
        {
            return entity.GetComponent<T>();
        }

        public bool HasComponent<T>()
        {
            return entity.HasComponent<T>();
        }

        public int ChildCount
        {
            get { return entity.ChildCount; }
        }

        public Entity GetChild(int index)
        {
            return entity.GetChild(index);
        }

        public T GetChildComponent<T>(int index)
        {
            Entity child = GetChild(index);
            if (child == null) return default;
            return child.GetComponent<T>();
        }
    }
}
