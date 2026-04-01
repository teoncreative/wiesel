using System;

namespace WieselEngine
{
    public enum LayoutDirection { None, Row, Column }
    public enum ChildAlignment { Start, Center, End }

    public class CanvasComponent : Component
    {

        public LayoutDirection Direction
        {
            get { return (LayoutDirection)Internals.Canvas_GetDirection(entity.ScenePtr, entity.Id); }
            set { Internals.Canvas_SetDirection(entity.ScenePtr, entity.Id, (int)value); }
        }

        public ChildAlignment Alignment
        {
            get { return (ChildAlignment)Internals.Canvas_GetAlignment(entity.ScenePtr, entity.Id); }
            set { Internals.Canvas_SetAlignment(entity.ScenePtr, entity.Id, (int)value); }
        }

        public float Spacing
        {
            get { return Internals.Canvas_GetSpacing(entity.ScenePtr, entity.Id); }
            set { Internals.Canvas_SetSpacing(entity.ScenePtr, entity.Id, value); }
        }

        public int SortOrder
        {
            get { return Internals.Canvas_GetSortOrder(entity.ScenePtr, entity.Id); }
            set { Internals.Canvas_SetSortOrder(entity.ScenePtr, entity.Id, value); }
        }

        public CanvasComponent(Entity entity)
        {
            this.entity = entity;
        }
    }
}
