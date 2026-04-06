namespace WieselEngine
{
    public enum LoadSceneMode
    {
        Single = 0,
        Additive = 1
    }

    public static class SceneManager
    {
        /// <summary>
        /// Load a scene synchronously by its registered name.
        /// Single mode replaces all loaded scenes. Additive loads alongside existing scenes.
        /// </summary>
        public static void LoadScene(string name, LoadSceneMode mode = LoadSceneMode.Single)
        {
            Internals.SceneManager_LoadScene(name, (int)mode);
        }

        /// <summary>
        /// Load a scene synchronously by its file path (VFS or filesystem).
        /// </summary>
        public static void LoadScenePath(string path, LoadSceneMode mode = LoadSceneMode.Single)
        {
            Internals.SceneManager_LoadScenePath(path, (int)mode);
        }

        /// <summary>
        /// Load a scene asynchronously (queued for next frame) by its registered name.
        /// </summary>
        public static void LoadSceneAsync(string name, LoadSceneMode mode = LoadSceneMode.Single)
        {
            Internals.SceneManager_LoadSceneAsync(name, (int)mode);
        }

        /// <summary>
        /// Load a scene asynchronously by its file path.
        /// </summary>
        public static void LoadSceneAsyncPath(string path, LoadSceneMode mode = LoadSceneMode.Single)
        {
            Internals.SceneManager_LoadSceneAsyncPath(path, (int)mode);
        }

        /// <summary>
        /// Load a scene via an intermediate loading screen scene.
        /// The loading scene is shown immediately, the target loads in the background.
        /// </summary>
        public static void LoadSceneWithLoading(string targetScene, string loadingScene)
        {
            Internals.SceneManager_LoadSceneWithLoading(targetScene, loadingScene);
        }

        /// <summary>
        /// Get the async loading progress (0.0 to 1.0).
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
        /// </summary>
        public static void ActivateLoadedScene()
        {
            Internals.SceneManager_ActivateLoadedScene();
        }

        /// <summary>
        /// Unload a specific additively-loaded scene by name (queued for end of frame).
        /// Cannot unload the active/primary scene.
        /// </summary>
        public static void UnloadScene(string name)
        {
            Internals.SceneManager_UnloadScene(name);
        }

        /// <summary>
        /// Get the number of currently loaded scenes (including the active scene).
        /// </summary>
        public static int LoadedSceneCount
        {
            get { return Internals.SceneManager_GetLoadedSceneCount(); }
        }

        /// <summary>
        /// Get the scene at the given index in the loaded scenes list.
        /// Returns null if the index is out of range.
        /// </summary>
        public static Scene GetLoadedScene(int index)
        {
            ulong ptr = Internals.SceneManager_GetLoadedScene(index);
            if (ptr == 0)
            {
                return null;
            }
            return new Scene(ptr);
        }

        /// <summary>
        /// Find a loaded scene by name. Returns null if not found.
        /// </summary>
        public static Scene FindScene(string name)
        {
            ulong ptr = Internals.SceneManager_FindScene(name);
            if (ptr == 0)
            {
                return null;
            }
            return new Scene(ptr);
        }

        /// <summary>
        /// Move an entity (and optionally its children) from one scene to another.
        /// The old entity handle becomes invalid. Returns the new Entity in the target scene.
        /// </summary>
        public static Entity MoveEntityToScene(Entity entity, Scene targetScene, bool moveChildren = true)
        {
            return Internals.SceneManager_MoveEntityToScene(entity.ScenePtr, entity.Id, targetScene.Ptr, moveChildren);
        }
    }
}
