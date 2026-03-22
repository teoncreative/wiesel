using System;

namespace WieselEngine
{
    public class TextComponent
    {
        private ulong scenePtr;
        private ulong entityId;
        private HandledVector4f color;

        public string Text
        {
            get { return Internals.Text_GetText(scenePtr, entityId); }
            set { Internals.Text_SetText(scenePtr, entityId, value); }
        }

        public float FontSize
        {
            get { return Internals.Text_GetFontSize(scenePtr, entityId); }
            set { Internals.Text_SetFontSize(scenePtr, entityId, value); }
        }

        public Vector4f Color
        {
            get { return color; }
            set { color.X = value.X; color.Y = value.Y; color.Z = value.Z; color.W = value.W; }
        }

        public TextComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
            this.color = new HandledVector4f(
                () => Internals.Text_GetColorR(scenePtr, entityId),
                (v) => Internals.Text_SetColorR(scenePtr, entityId, v),
                () => Internals.Text_GetColorG(scenePtr, entityId),
                (v) => Internals.Text_SetColorG(scenePtr, entityId, v),
                () => Internals.Text_GetColorB(scenePtr, entityId),
                (v) => Internals.Text_SetColorB(scenePtr, entityId, v),
                () => Internals.Text_GetColorA(scenePtr, entityId),
                (v) => Internals.Text_SetColorA(scenePtr, entityId, v));
        }
    }
}
