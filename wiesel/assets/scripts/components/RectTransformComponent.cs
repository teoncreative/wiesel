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

    public class RectTransformComponent : Component
    {
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
            get { return Internals.RectTransform_GetRotation(entity.ScenePtr, entity.Id); }
            set { Internals.RectTransform_SetRotation(entity.ScenePtr, entity.Id, value); }
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
            get { return (AnchorPreset)Internals.RectTransform_GetAnchor(entity.ScenePtr, entity.Id); }
            set { Internals.RectTransform_SetAnchor(entity.ScenePtr, entity.Id, (int)value); }
        }

        public AnchorPreset Pivot
        {
            get { return (AnchorPreset)Internals.RectTransform_GetPivot(entity.ScenePtr, entity.Id); }
            set { Internals.RectTransform_SetPivot(entity.ScenePtr, entity.Id, (int)value); }
        }

        public SizeMode SizeModeX
        {
            get { return (SizeMode)Internals.RectTransform_GetSizeModeX(entity.ScenePtr, entity.Id); }
            set { Internals.RectTransform_SetSizeModeX(entity.ScenePtr, entity.Id, (int)value); }
        }

        public SizeMode SizeModeY
        {
            get { return (SizeMode)Internals.RectTransform_GetSizeModeY(entity.ScenePtr, entity.Id); }
            set { Internals.RectTransform_SetSizeModeY(entity.ScenePtr, entity.Id, (int)value); }
        }

        public Vector4f Padding
        {
            get { return padding; }
            set { padding.X = value.X; padding.Y = value.Y; padding.Z = value.Z; padding.W = value.W; }
        }

        public Vector2f ComputedPosition
        {
            get { return new Vector2f(
                Internals.RectTransform_GetComputedPositionX(entity.ScenePtr, entity.Id),
                Internals.RectTransform_GetComputedPositionY(entity.ScenePtr, entity.Id)); }
        }

        public Vector2f ComputedSize
        {
            get { return new Vector2f(
                Internals.RectTransform_GetComputedSizeX(entity.ScenePtr, entity.Id),
                Internals.RectTransform_GetComputedSizeY(entity.ScenePtr, entity.Id)); }
        }

        public RectTransformComponent(Entity entity)
        {
            this.entity = entity;
            this.position = new HandledVector2f(
                () => Internals.RectTransform_GetPositionX(entity.ScenePtr, entity.Id),
                (v) => Internals.RectTransform_SetPositionX(entity.ScenePtr, entity.Id, v),
                () => Internals.RectTransform_GetPositionY(entity.ScenePtr, entity.Id),
                (v) => Internals.RectTransform_SetPositionY(entity.ScenePtr, entity.Id, v));
            this.size = new HandledVector2f(
                () => Internals.RectTransform_GetSizeX(entity.ScenePtr, entity.Id),
                (v) => Internals.RectTransform_SetSizeX(entity.ScenePtr, entity.Id, v),
                () => Internals.RectTransform_GetSizeY(entity.ScenePtr, entity.Id),
                (v) => Internals.RectTransform_SetSizeY(entity.ScenePtr, entity.Id, v));
            this.scale = new HandledVector2f(
                () => Internals.RectTransform_GetScaleX(entity.ScenePtr, entity.Id),
                (v) => Internals.RectTransform_SetScaleX(entity.ScenePtr, entity.Id, v),
                () => Internals.RectTransform_GetScaleY(entity.ScenePtr, entity.Id),
                (v) => Internals.RectTransform_SetScaleY(entity.ScenePtr, entity.Id, v));
            this.padding = new HandledVector4f(
                () => Internals.RectTransform_GetPaddingLeft(entity.ScenePtr, entity.Id),
                (v) => Internals.RectTransform_SetPaddingLeft(entity.ScenePtr, entity.Id, v),
                () => Internals.RectTransform_GetPaddingTop(entity.ScenePtr, entity.Id),
                (v) => Internals.RectTransform_SetPaddingTop(entity.ScenePtr, entity.Id, v),
                () => Internals.RectTransform_GetPaddingRight(entity.ScenePtr, entity.Id),
                (v) => Internals.RectTransform_SetPaddingRight(entity.ScenePtr, entity.Id, v),
                () => Internals.RectTransform_GetPaddingBottom(entity.ScenePtr, entity.Id),
                (v) => Internals.RectTransform_SetPaddingBottom(entity.ScenePtr, entity.Id, v));
        }
    }
}
