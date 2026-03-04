using System;

namespace WieselEngine
{
    public class CanvasRectComponent
    {
        private ulong scenePtr;
        private ulong entityId;
        private HandledVector4f color;

        public Vector4f Color
        {
            get { return color; }
            set { color.X = value.X; color.Y = value.Y; color.Z = value.Z; color.W = value.W; }
        }

        public CanvasRectComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
            this.color = new HandledVector4f(
                () => Internals.CanvasRect_GetColorR(scenePtr, entityId),
                (v) => Internals.CanvasRect_SetColorR(scenePtr, entityId, v),
                () => Internals.CanvasRect_GetColorG(scenePtr, entityId),
                (v) => Internals.CanvasRect_SetColorG(scenePtr, entityId, v),
                () => Internals.CanvasRect_GetColorB(scenePtr, entityId),
                (v) => Internals.CanvasRect_SetColorB(scenePtr, entityId, v),
                () => Internals.CanvasRect_GetColorA(scenePtr, entityId),
                (v) => Internals.CanvasRect_SetColorA(scenePtr, entityId, v));
        }
    }
}
