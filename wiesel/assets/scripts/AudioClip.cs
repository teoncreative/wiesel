namespace WieselEngine
{
    public class AudioClip
    {
        // Stored as asset handle UUID string internally
        public string handle = "";

        public string Handle
        {
            get { return handle; }
            set { handle = value; }
        }

        public bool IsValid()
        {
            return !string.IsNullOrEmpty(handle);
        }
    }
}