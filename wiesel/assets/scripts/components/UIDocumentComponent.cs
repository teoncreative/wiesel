namespace WieselEngine
{
    public class UIDocumentComponent : Component
    {
        public UIDocumentComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        public void SetInt(string name, int value)
        {
            Internals.UIDocument_SetInt(scenePtr, entityId, name, value);
        }

        public int GetInt(string name)
        {
            return Internals.UIDocument_GetInt(scenePtr, entityId, name);
        }

        public void SetFloat(string name, float value)
        {
            Internals.UIDocument_SetFloat(scenePtr, entityId, name, value);
        }

        public float GetFloat(string name)
        {
            return Internals.UIDocument_GetFloat(scenePtr, entityId, name);
        }

        public void SetString(string name, string value)
        {
            Internals.UIDocument_SetString(scenePtr, entityId, name, value);
        }

        public string GetString(string name)
        {
            return Internals.UIDocument_GetString(scenePtr, entityId, name);
        }

        public void SetBool(string name, bool value)
        {
            Internals.UIDocument_SetBool(scenePtr, entityId, name, value);
        }

        public bool GetBool(string name)
        {
            return Internals.UIDocument_GetBool(scenePtr, entityId, name);
        }
    }
}
