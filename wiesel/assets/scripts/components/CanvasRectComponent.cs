using System;

namespace WieselEngine
{
    public class CanvasRectComponent : Component
    {
        private HandledVector4f color;

        public Vector4f Color
        {
            get { return color; }
            set { color.X = value.X; color.Y = value.Y; color.Z = value.Z; color.W = value.W; }
        }

        public CanvasRectComponent(Entity entity)
        {
            this.entity = entity;
            this.color = new HandledVector4f(
                () => Internals.CanvasRect_GetColorR(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasRect_SetColorR(entity.ScenePtr, entity.Id, v),
                () => Internals.CanvasRect_GetColorG(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasRect_SetColorG(entity.ScenePtr, entity.Id, v),
                () => Internals.CanvasRect_GetColorB(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasRect_SetColorB(entity.ScenePtr, entity.Id, v),
                () => Internals.CanvasRect_GetColorA(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasRect_SetColorA(entity.ScenePtr, entity.Id, v));
        }
    }
}
