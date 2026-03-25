namespace WieselEngine
{
    public class SpriteRendererComponent
    {
        private ulong scenePtr;
        private ulong entityId;

        public SpriteRendererComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        public bool FlipX
        {
            get { return Internals.SpriteRenderer_GetFlipX(scenePtr, entityId); }
            set { Internals.SpriteRenderer_SetFlipX(scenePtr, entityId, value); }
        }

        public bool FlipY
        {
            get { return Internals.SpriteRenderer_GetFlipY(scenePtr, entityId); }
            set { Internals.SpriteRenderer_SetFlipY(scenePtr, entityId, value); }
        }

        public Vector4f Tint
        {
            get
            {
                return new Vector4f(
                    Internals.SpriteRenderer_GetTintR(scenePtr, entityId),
                    Internals.SpriteRenderer_GetTintG(scenePtr, entityId),
                    Internals.SpriteRenderer_GetTintB(scenePtr, entityId),
                    Internals.SpriteRenderer_GetTintA(scenePtr, entityId));
            }
            set
            {
                Internals.SpriteRenderer_SetTintR(scenePtr, entityId, value.X);
                Internals.SpriteRenderer_SetTintG(scenePtr, entityId, value.Y);
                Internals.SpriteRenderer_SetTintB(scenePtr, entityId, value.Z);
                Internals.SpriteRenderer_SetTintA(scenePtr, entityId, value.W);
            }
        }

        public int SortLayer
        {
            get { return Internals.SpriteRenderer_GetSortLayer(scenePtr, entityId); }
            set { Internals.SpriteRenderer_SetSortLayer(scenePtr, entityId, value); }
        }
    }
}
