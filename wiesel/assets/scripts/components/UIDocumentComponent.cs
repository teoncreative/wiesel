namespace WieselEngine
{
    public class UIDocumentComponent : Component
    {
        public UIDocumentComponent(Entity entity)
        {
            this.entity = entity;
        }

        public bool Visible
        {
            get { return Internals.UIDocument_GetVisible(entity.ScenePtr, entity.Id); }
            set { Internals.UIDocument_SetVisible(entity.ScenePtr, entity.Id, value); }
        }

        public void SetInt(string name, int value)
        {
            Internals.UIDocument_SetInt(entity.ScenePtr, entity.Id, name, value);
        }

        public int GetInt(string name)
        {
            return Internals.UIDocument_GetInt(entity.ScenePtr, entity.Id, name);
        }

        public void SetFloat(string name, float value)
        {
            Internals.UIDocument_SetFloat(entity.ScenePtr, entity.Id, name, value);
        }

        public float GetFloat(string name)
        {
            return Internals.UIDocument_GetFloat(entity.ScenePtr, entity.Id, name);
        }

        public void SetString(string name, string value)
        {
            Internals.UIDocument_SetString(entity.ScenePtr, entity.Id, name, value);
        }

        public string GetString(string name)
        {
            return Internals.UIDocument_GetString(entity.ScenePtr, entity.Id, name);
        }

        public void SetBool(string name, bool value)
        {
            Internals.UIDocument_SetBool(entity.ScenePtr, entity.Id, name, value);
        }

        public bool GetBool(string name)
        {
            return Internals.UIDocument_GetBool(entity.ScenePtr, entity.Id, name);
        }
    }
}
