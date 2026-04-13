namespace WieselEngine
{
    public class SpriteRendererComponent : Component
    {

        public SpriteRendererComponent(Entity entity) : base(entity) { }

        public bool FlipX
        {
            get { return Internals.SpriteRenderer_GetFlipX(entity.ScenePtr, entity.Id); }
            set { Internals.SpriteRenderer_SetFlipX(entity.ScenePtr, entity.Id, value); }
        }

        public bool FlipY
        {
            get { return Internals.SpriteRenderer_GetFlipY(entity.ScenePtr, entity.Id); }
            set { Internals.SpriteRenderer_SetFlipY(entity.ScenePtr, entity.Id, value); }
        }

        public Vector4f Tint
        {
            get
            {
                return new Vector4f(
                    Internals.SpriteRenderer_GetTintR(entity.ScenePtr, entity.Id),
                    Internals.SpriteRenderer_GetTintG(entity.ScenePtr, entity.Id),
                    Internals.SpriteRenderer_GetTintB(entity.ScenePtr, entity.Id),
                    Internals.SpriteRenderer_GetTintA(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.SpriteRenderer_SetTintR(entity.ScenePtr, entity.Id, value.X);
                Internals.SpriteRenderer_SetTintG(entity.ScenePtr, entity.Id, value.Y);
                Internals.SpriteRenderer_SetTintB(entity.ScenePtr, entity.Id, value.Z);
                Internals.SpriteRenderer_SetTintA(entity.ScenePtr, entity.Id, value.W);
            }
        }

        public int SortLayer
        {
            get { return Internals.SpriteRenderer_GetSortLayer(entity.ScenePtr, entity.Id); }
            set { Internals.SpriteRenderer_SetSortLayer(entity.ScenePtr, entity.Id, value); }
        }
    }
}
