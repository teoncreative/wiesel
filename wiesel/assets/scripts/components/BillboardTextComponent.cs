namespace WieselEngine
{
    public enum TextAlignment
    {
        Left = 0,
        Center = 1,
        Right = 2,
    }

    public class BillboardTextComponent : Component
    {
        public BillboardTextComponent(Entity entity) : base(entity) { }

        public string Text
        {
            get { return Internals.BillboardText_GetText(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardText_SetText(entity.ScenePtr, entity.Id, value); }
        }

        public Font Font
        {
            get
            {
                Font f = new Font();
                f.Handle = Internals.BillboardText_GetFontHandle(entity.ScenePtr, entity.Id);
                return f;
            }
            set
            {
                Internals.BillboardText_SetFontHandle(entity.ScenePtr, entity.Id,
                    value != null ? value.Handle : "");
            }
        }

        public float FontSize
        {
            get { return Internals.BillboardText_GetFontSize(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardText_SetFontSize(entity.ScenePtr, entity.Id, value); }
        }

        public Vector4f Color
        {
            get
            {
                return new Vector4f(
                    Internals.BillboardText_GetColorR(entity.ScenePtr, entity.Id),
                    Internals.BillboardText_GetColorG(entity.ScenePtr, entity.Id),
                    Internals.BillboardText_GetColorB(entity.ScenePtr, entity.Id),
                    Internals.BillboardText_GetColorA(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.BillboardText_SetColorR(entity.ScenePtr, entity.Id, value.X);
                Internals.BillboardText_SetColorG(entity.ScenePtr, entity.Id, value.Y);
                Internals.BillboardText_SetColorB(entity.ScenePtr, entity.Id, value.Z);
                Internals.BillboardText_SetColorA(entity.ScenePtr, entity.Id, value.W);
            }
        }

        public TextAlignment Alignment
        {
            get { return (TextAlignment)Internals.BillboardText_GetAlignment(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardText_SetAlignment(entity.ScenePtr, entity.Id, (int)value); }
        }

        public float MinSize
        {
            get { return Internals.BillboardText_GetMinSize(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardText_SetMinSize(entity.ScenePtr, entity.Id, value); }
        }

        public float MaxSize
        {
            get { return Internals.BillboardText_GetMaxSize(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardText_SetMaxSize(entity.ScenePtr, entity.Id, value); }
        }

        public int SortLayer
        {
            get { return Internals.BillboardText_GetSortLayer(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardText_SetSortLayer(entity.ScenePtr, entity.Id, value); }
        }

        public BillboardOcclusion Occlusion
        {
            get { return (BillboardOcclusion)Internals.BillboardText_GetOcclusion(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardText_SetOcclusion(entity.ScenePtr, entity.Id, (int)value); }
        }

        public float OccludedAlpha
        {
            get { return Internals.BillboardText_GetOccludedAlpha(entity.ScenePtr, entity.Id); }
            set { Internals.BillboardText_SetOccludedAlpha(entity.ScenePtr, entity.Id, value); }
        }
    }
}
