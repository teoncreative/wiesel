namespace WieselEngine
{
    public class AudioClip
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
    }
}