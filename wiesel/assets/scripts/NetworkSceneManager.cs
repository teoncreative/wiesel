namespace WieselEngine
{
    public static class NetworkSceneManager
    {
        public static void LoadScene(string name, LoadSceneMode mode = LoadSceneMode.Additive)
        {
            Internals.NetworkSceneManager_LoadScene(name, (int)mode);
        }

        public static void LoadSceneWithLoading(string targetScene, string loadingScene)
        {
            Internals.NetworkSceneManager_LoadSceneWithLoading(targetScene, loadingScene);
        }
    }
}
