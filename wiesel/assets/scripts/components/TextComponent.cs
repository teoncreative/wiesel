using System;

namespace WieselEngine
{
    public class TextComponent : Component
    {
        private HandledVector4f color;

        public string Text
        {
            get { return Internals.Text_GetText(entity.ScenePtr, entity.Id); }
            set { Internals.Text_SetText(entity.ScenePtr, entity.Id, value); }
        }

        public float FontSize
        {
            get { return Internals.Text_GetFontSize(entity.ScenePtr, entity.Id); }
            set { Internals.Text_SetFontSize(entity.ScenePtr, entity.Id, value); }
        }

        public Vector4f Color
        {
            get { return color; }
            set { color.X = value.X; color.Y = value.Y; color.Z = value.Z; color.W = value.W; }
        }

        public TextComponent(Entity entity)
        {
            this.entity = entity;
            this.color = new HandledVector4f(
                () => Internals.Text_GetColorR(entity.ScenePtr, entity.Id),
                (v) => Internals.Text_SetColorR(entity.ScenePtr, entity.Id, v),
                () => Internals.Text_GetColorG(entity.ScenePtr, entity.Id),
                (v) => Internals.Text_SetColorG(entity.ScenePtr, entity.Id, v),
                () => Internals.Text_GetColorB(entity.ScenePtr, entity.Id),
                (v) => Internals.Text_SetColorB(entity.ScenePtr, entity.Id, v),
                () => Internals.Text_GetColorA(entity.ScenePtr, entity.Id),
                (v) => Internals.Text_SetColorA(entity.ScenePtr, entity.Id, v));
        }
    }
}
