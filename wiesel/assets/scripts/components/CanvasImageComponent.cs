using System;

namespace WieselEngine
{
    public class CanvasImageComponent : Component
    {
        private HandledVector4f tint;
        private HandledVector4f uvRect;

        public Vector4f Tint
        {
            get { return tint; }
            set { tint.X = value.X; tint.Y = value.Y; tint.Z = value.Z; tint.W = value.W; }
        }

        public Vector4f UVRect
        {
            get { return uvRect; }
            set { uvRect.X = value.X; uvRect.Y = value.Y; uvRect.Z = value.Z; uvRect.W = value.W; }
        }

        public CanvasImageComponent(Entity entity)
        {
            this.entity = entity;
            this.tint = new HandledVector4f(
                () => Internals.CanvasImage_GetTintR(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasImage_SetTintR(entity.ScenePtr, entity.Id, v),
                () => Internals.CanvasImage_GetTintG(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasImage_SetTintG(entity.ScenePtr, entity.Id, v),
                () => Internals.CanvasImage_GetTintB(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasImage_SetTintB(entity.ScenePtr, entity.Id, v),
                () => Internals.CanvasImage_GetTintA(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasImage_SetTintA(entity.ScenePtr, entity.Id, v));
            this.uvRect = new HandledVector4f(
                () => Internals.CanvasImage_GetUVRectX(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasImage_SetUVRectX(entity.ScenePtr, entity.Id, v),
                () => Internals.CanvasImage_GetUVRectY(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasImage_SetUVRectY(entity.ScenePtr, entity.Id, v),
                () => Internals.CanvasImage_GetUVRectZ(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasImage_SetUVRectZ(entity.ScenePtr, entity.Id, v),
                () => Internals.CanvasImage_GetUVRectW(entity.ScenePtr, entity.Id),
                (v) => Internals.CanvasImage_SetUVRectW(entity.ScenePtr, entity.Id, v));
        }
    }
}
