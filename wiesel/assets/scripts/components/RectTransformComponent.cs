using System;

namespace WieselEngine
{
    public enum AnchorPreset
    {
        TopLeft, TopCenter, TopRight,
        MiddleLeft, MiddleCenter, MiddleRight,
        BottomLeft, BottomCenter, BottomRight,
        StretchAll
    }

    public enum SizeMode { Fixed, Percent }

    public class RectTransformComponent
    {
        private ulong scenePtr;
        private ulong entityId;
        private HandledVector2f position;
        private HandledVector2f size;
        private HandledVector2f scale;
        private HandledVector4f padding;

        public Vector2f Position
        {
            get { return position; }
            set { position.X = value.X; position.Y = value.Y; }
        }

        public float Rotation
        {
            get { return Internals.RectTransform_GetRotation(scenePtr, entityId); }
            set { Internals.RectTransform_SetRotation(scenePtr, entityId, value); }
        }

        public Vector2f Size
        {
            get { return size; }
            set { size.X = value.X; size.Y = value.Y; }
        }

        public Vector2f Scale
        {
            get { return scale; }
            set { scale.X = value.X; scale.Y = value.Y; }
        }

        public AnchorPreset Anchor
        {
            get { return (AnchorPreset)Internals.RectTransform_GetAnchor(scenePtr, entityId); }
            set { Internals.RectTransform_SetAnchor(scenePtr, entityId, (int)value); }
        }

        public AnchorPreset Pivot
        {
            get { return (AnchorPreset)Internals.RectTransform_GetPivot(scenePtr, entityId); }
            set { Internals.RectTransform_SetPivot(scenePtr, entityId, (int)value); }
        }

        public SizeMode SizeModeX
        {
            get { return (SizeMode)Internals.RectTransform_GetSizeModeX(scenePtr, entityId); }
            set { Internals.RectTransform_SetSizeModeX(scenePtr, entityId, (int)value); }
        }

        public SizeMode SizeModeY
        {
            get { return (SizeMode)Internals.RectTransform_GetSizeModeY(scenePtr, entityId); }
            set { Internals.RectTransform_SetSizeModeY(scenePtr, entityId, (int)value); }
        }

        public Vector4f Padding
        {
            get { return padding; }
            set { padding.X = value.X; padding.Y = value.Y; padding.Z = value.Z; padding.W = value.W; }
        }

        public Vector2f ComputedPosition
        {
            get { return new Vector2f(
                Internals.RectTransform_GetComputedPositionX(scenePtr, entityId),
                Internals.RectTransform_GetComputedPositionY(scenePtr, entityId)); }
        }

        public Vector2f ComputedSize
        {
            get { return new Vector2f(
                Internals.RectTransform_GetComputedSizeX(scenePtr, entityId),
                Internals.RectTransform_GetComputedSizeY(scenePtr, entityId)); }
        }

        public RectTransformComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
            this.position = new HandledVector2f(
                () => Internals.RectTransform_GetPositionX(scenePtr, entityId),
                (v) => Internals.RectTransform_SetPositionX(scenePtr, entityId, v),
                () => Internals.RectTransform_GetPositionY(scenePtr, entityId),
                (v) => Internals.RectTransform_SetPositionY(scenePtr, entityId, v));
            this.size = new HandledVector2f(
                () => Internals.RectTransform_GetSizeX(scenePtr, entityId),
                (v) => Internals.RectTransform_SetSizeX(scenePtr, entityId, v),
                () => Internals.RectTransform_GetSizeY(scenePtr, entityId),
                (v) => Internals.RectTransform_SetSizeY(scenePtr, entityId, v));
            this.scale = new HandledVector2f(
                () => Internals.RectTransform_GetScaleX(scenePtr, entityId),
                (v) => Internals.RectTransform_SetScaleX(scenePtr, entityId, v),
                () => Internals.RectTransform_GetScaleY(scenePtr, entityId),
                (v) => Internals.RectTransform_SetScaleY(scenePtr, entityId, v));
            this.padding = new HandledVector4f(
                () => Internals.RectTransform_GetPaddingLeft(scenePtr, entityId),
                (v) => Internals.RectTransform_SetPaddingLeft(scenePtr, entityId, v),
                () => Internals.RectTransform_GetPaddingTop(scenePtr, entityId),
                (v) => Internals.RectTransform_SetPaddingTop(scenePtr, entityId, v),
                () => Internals.RectTransform_GetPaddingRight(scenePtr, entityId),
                (v) => Internals.RectTransform_SetPaddingRight(scenePtr, entityId, v),
                () => Internals.RectTransform_GetPaddingBottom(scenePtr, entityId),
                (v) => Internals.RectTransform_SetPaddingBottom(scenePtr, entityId, v));
        }
    }
}
