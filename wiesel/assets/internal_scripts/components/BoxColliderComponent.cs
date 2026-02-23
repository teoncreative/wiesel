using System;

namespace WieselEngine
{
    public class BoxColliderComponent
    {
        private ulong scenePtr;
        private ulong entityId;

        public Vector3f Offset
        {
            get
            {
                return new Vector3f(
                    Internals.BoxCollider_GetOffsetX(scenePtr, entityId),
                    Internals.BoxCollider_GetOffsetY(scenePtr, entityId),
                    Internals.BoxCollider_GetOffsetZ(scenePtr, entityId));
            }
            set
            {
                Internals.BoxCollider_SetOffsetX(scenePtr, entityId, value.X);
                Internals.BoxCollider_SetOffsetY(scenePtr, entityId, value.Y);
                Internals.BoxCollider_SetOffsetZ(scenePtr, entityId, value.Z);
            }
        }

        public Vector3f HalfExtents
        {
            get
            {
                return new Vector3f(
                    Internals.BoxCollider_GetHalfExtentsX(scenePtr, entityId),
                    Internals.BoxCollider_GetHalfExtentsY(scenePtr, entityId),
                    Internals.BoxCollider_GetHalfExtentsZ(scenePtr, entityId));
            }
            set
            {
                Internals.BoxCollider_SetHalfExtentsX(scenePtr, entityId, value.X);
                Internals.BoxCollider_SetHalfExtentsY(scenePtr, entityId, value.Y);
                Internals.BoxCollider_SetHalfExtentsZ(scenePtr, entityId, value.Z);
            }
        }

        public bool IsTrigger
        {
            get { return Internals.BoxCollider_GetIsTrigger(scenePtr, entityId); }
            set { Internals.BoxCollider_SetIsTrigger(scenePtr, entityId, value); }
        }

        public BoxColliderComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }
    }
}
