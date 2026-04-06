namespace WieselEngine
{
    public class Scene
    {
        private ulong scenePtr;

        internal ulong Ptr { get { return scenePtr; } }

        internal Scene(ulong scenePtr)
        {
            this.scenePtr = scenePtr;
        }

        public string Name
        {
            get { return Internals.Scene_GetName(scenePtr); }
        }

        public Entity CreateEntity(string name)
        {
            return Internals.Scene_CreateEntity(scenePtr, name);
        }

        public Entity FindEntity(string name)
        {
            return Internals.Scene_FindEntity(scenePtr, name);
        }

        public Entity[] FindEntitiesByTag(string tag)
        {
            Entity[] result = Internals.Scene_FindEntitiesByTag(scenePtr, tag);
            if (result == null)
            {
                return new Entity[0];
            }
            return result;
        }

        public void DestroyEntity(Entity entity)
        {
            Internals.Scene_DestroyEntity(scenePtr, entity.Id);
        }

        public bool IsValid
        {
            get { return scenePtr != 0; }
        }

        public override bool Equals(object obj)
        {
            if (obj is Scene other)
            {
                return scenePtr == other.scenePtr;
            }
            return false;
        }

        public override int GetHashCode()
        {
            return scenePtr.GetHashCode();
        }

        public static bool operator ==(Scene a, Scene b)
        {
            if (ReferenceEquals(a, b))
            {
                return true;
            }
            if (ReferenceEquals(a, null) || ReferenceEquals(b, null))
            {
                return false;
            }
            return a.scenePtr == b.scenePtr;
        }

        public static bool operator !=(Scene a, Scene b)
        {
            return !(a == b);
        }
    }
}
