namespace WieselEngine
{
    public static class SceneManager
    {
        /// <summary>
        /// Load a scene by its registered name.
        /// The scene will be loaded at the start of the next frame.
        /// </summary>
        public static void LoadScene(string name)
        {
            Internals.SceneManager_LoadScene(name);
        }

        /// <summary>
        /// Load a scene by its file path (VFS or filesystem).
        /// The scene will be loaded at the start of the next frame.
        /// </summary>
        public static void LoadScenePath(string path)
        {
            Internals.SceneManager_LoadScenePath(path);
        }
    }
}