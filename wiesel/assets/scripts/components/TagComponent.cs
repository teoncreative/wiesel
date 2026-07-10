namespace WieselEngine
{
    public class TagComponent : Component
    {
        public TagComponent(Entity entity) : base(entity) { }

        public string Name
        {
            get { return Internals.TagComponent_GetName(entity.ScenePtr, entity.Id); }
            set { Internals.TagComponent_SetName(entity.ScenePtr, entity.Id, value); }
        }
    }
}
