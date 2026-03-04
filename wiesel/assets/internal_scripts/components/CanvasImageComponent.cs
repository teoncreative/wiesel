using System;

namespace WieselEngine
{
    public class CanvasImageComponent
    {
        private ulong scenePtr;
        private ulong entityId;
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

        public CanvasImageComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
            this.tint = new HandledVector4f(
                () => Internals.CanvasImage_GetTintR(scenePtr, entityId),
                (v) => Internals.CanvasImage_SetTintR(scenePtr, entityId, v),
                () => Internals.CanvasImage_GetTintG(scenePtr, entityId),
                (v) => Internals.CanvasImage_SetTintG(scenePtr, entityId, v),
                () => Internals.CanvasImage_GetTintB(scenePtr, entityId),
                (v) => Internals.CanvasImage_SetTintB(scenePtr, entityId, v),
                () => Internals.CanvasImage_GetTintA(scenePtr, entityId),
                (v) => Internals.CanvasImage_SetTintA(scenePtr, entityId, v));
            this.uvRect = new HandledVector4f(
                () => Internals.CanvasImage_GetUVRectX(scenePtr, entityId),
                (v) => Internals.CanvasImage_SetUVRectX(scenePtr, entityId, v),
                () => Internals.CanvasImage_GetUVRectY(scenePtr, entityId),
                (v) => Internals.CanvasImage_SetUVRectY(scenePtr, entityId, v),
                () => Internals.CanvasImage_GetUVRectZ(scenePtr, entityId),
                (v) => Internals.CanvasImage_SetUVRectZ(scenePtr, entityId, v),
                () => Internals.CanvasImage_GetUVRectW(scenePtr, entityId),
                (v) => Internals.CanvasImage_SetUVRectW(scenePtr, entityId, v));
        }
    }
}
