namespace WieselEngine
{
    public enum BillboardOcclusion
    {
        Disabled = 0,
        Faded = 1,
        AlwaysVisible = 2,
    }

    public class BillboardRendererComponent : Component
    {
        public BillboardRendererComponent(Entity entity) : base(entity) { }

        public Vector2f Size
        {
            get
            {
                return new Vector2f(
                    Internals.BillboardRenderer_GetSizeX(entity.ScenePtr, entity.Id),
                    Internals.BillboardRenderer_GetSizeY(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.BillboardRenderer_SetSizeX(entity.ScenePtr, entity.Id, value.X);
                Internals.BillboardRenderer_SetSizeY(entity.ScenePtr, entity.Id, value.Y);
            }
        }

        public float MinSize
        {
            get { return Internals.BillboardRenderer_GetMinSize(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardRenderer_SetMinSize(entity.ScenePtr, entity.Id, value); }
        }

        public float MaxSize
        {
            get { return Internals.BillboardRenderer_GetMaxSize(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardRenderer_SetMaxSize(entity.ScenePtr, entity.Id, value); }
        }

        public Vector2f Pivot
        {
            get
            {
                return new Vector2f(
                    Internals.BillboardRenderer_GetPivotX(entity.ScenePtr, entity.Id),
                    Internals.BillboardRenderer_GetPivotY(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.BillboardRenderer_SetPivotX(entity.ScenePtr, entity.Id, value.X);
                Internals.BillboardRenderer_SetPivotY(entity.ScenePtr, entity.Id, value.Y);
            }
        }

        public Vector4f Tint
        {
            get
            {
                return new Vector4f(
                    Internals.BillboardRenderer_GetTintR(entity.ScenePtr, entity.Id),
                    Internals.BillboardRenderer_GetTintG(entity.ScenePtr, entity.Id),
                    Internals.BillboardRenderer_GetTintB(entity.ScenePtr, entity.Id),
                    Internals.BillboardRenderer_GetTintA(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.BillboardRenderer_SetTintR(entity.ScenePtr, entity.Id, value.X);
                Internals.BillboardRenderer_SetTintG(entity.ScenePtr, entity.Id, value.Y);
                Internals.BillboardRenderer_SetTintB(entity.ScenePtr, entity.Id, value.Z);
                Internals.BillboardRenderer_SetTintA(entity.ScenePtr, entity.Id, value.W);
            }
        }

        public int SortLayer
        {
            get { return Internals.BillboardRenderer_GetSortLayer(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardRenderer_SetSortLayer(entity.ScenePtr, entity.Id, value); }
        }

        public BillboardOcclusion Occlusion
        {
            get { return (BillboardOcclusion)Internals.BillboardRenderer_GetOcclusion(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardRenderer_SetOcclusion(entity.ScenePtr, entity.Id, (int)value); }
        }

        public float OccludedAlpha
        {
            get { return Internals.BillboardRenderer_GetOccludedAlpha(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardRenderer_SetOccludedAlpha(entity.ScenePtr, entity.Id, value); }
        }
    }
}
