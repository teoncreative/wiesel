namespace WieselEngine
{
    public class Prefab
    {
        private string handle = "";

        public string Handle
        {
            get { return handle; }
            set { handle = value; }
        }

        public bool IsValid()
        {
            return !string.IsNullOrEmpty(handle);
        }

        public Entity Instantiate(Scene scene)
        {
            if (!IsValid())
            {
                return null;
            }
            return Internals.Prefab_Instantiate(scene.Ptr, handle);
        }
    }
}
