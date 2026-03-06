namespace WieselEngine
{
    public static class Prefab
    {
        /// <summary>
        /// Instantiate a prefab from a file path into the current scene.
        /// Returns the entity handle of the root entity, or 0 if failed.
        /// </summary>
        public static ulong Instantiate(ulong scenePtr, string path)
        {
            return Internals.Prefab_Instantiate(scenePtr, path);
        }
    }
}