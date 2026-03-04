using System;

namespace WieselEngine
{
    public enum LayoutDirection { None, Row, Column }
    public enum ChildAlignment { Start, Center, End }

    public class CanvasComponent
    {
        private ulong scenePtr;
        private ulong entityId;

        public LayoutDirection Direction
        {
            get { return (LayoutDirection)Internals.Canvas_GetDirection(scenePtr, entityId); }
            set { Internals.Canvas_SetDirection(scenePtr, entityId, (int)value); }
        }

        public ChildAlignment Alignment
        {
            get { return (ChildAlignment)Internals.Canvas_GetAlignment(scenePtr, entityId); }
            set { Internals.Canvas_SetAlignment(scenePtr, entityId, (int)value); }
        }

        public float Spacing
        {
            get { return Internals.Canvas_GetSpacing(scenePtr, entityId); }
            set { Internals.Canvas_SetSpacing(scenePtr, entityId, value); }
        }

        public int SortOrder
        {
            get { return Internals.Canvas_GetSortOrder(scenePtr, entityId); }
            set { Internals.Canvas_SetSortOrder(scenePtr, entityId, value); }
        }

        public CanvasComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }
    }
}
