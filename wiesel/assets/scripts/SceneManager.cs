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

        /// <summary>
        /// Load a scene via an intermediate loading screen scene.
        /// The loading scene is shown immediately, the target loads in the background.
        /// </summary>
        public static void LoadScene(string targetScene, string loadingScene)
        {
            Internals.SceneManager_LoadSceneWithLoading(targetScene, loadingScene);
        }

        /// <summary>
        /// Get the async loading progress (0.0 to 1.0).
        /// Query this from your loading screen script to update a progress bar.
        /// </summary>
        public static float LoadProgress
        {
            get { return Internals.SceneManager_GetLoadProgress(); }
        }

        /// <summary>
        /// Check if the target scene is fully loaded and ready to activate.
        /// </summary>
        public static bool IsSceneReady
        {
            get { return Internals.SceneManager_IsSceneReady(); }
        }

        /// <summary>
        /// Switch to the loaded target scene.
        /// Call this from your loading screen when IsSceneReady is true.
        /// </summary>
        public static void ActivateLoadedScene()
        {
            Internals.SceneManager_ActivateLoadedScene();
        }
    }
}