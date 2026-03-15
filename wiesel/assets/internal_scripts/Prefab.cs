namespace WieselEngine
{
    public class Prefab
    {
        private string path = "";

        public string Path
        {
            get { return path; }
            set { path = value; }
        }

        public Entity Instantiate(ulong scenePtr)
        {
            if (string.IsNullOrEmpty(path)) return null;
            ulong id = Internals.Prefab_Instantiate(scenePtr, path);
            if (id == 0) return null;
            return new Entity(scenePtr, id);
        }
    }
}
