using System;

namespace WieselEngine
{
    public class SphereColliderComponent : Component
    {

        public Vector3f Offset
        {
            get
            {
                return new Vector3f(
                    Internals.SphereCollider_GetOffsetX(scenePtr, entityId),
                    Internals.SphereCollider_GetOffsetY(scenePtr, entityId),
                    Internals.SphereCollider_GetOffsetZ(scenePtr, entityId));
            }
            set
            {
                Internals.SphereCollider_SetOffsetX(scenePtr, entityId, value.X);
                Internals.SphereCollider_SetOffsetY(scenePtr, entityId, value.Y);
                Internals.SphereCollider_SetOffsetZ(scenePtr, entityId, value.Z);
            }
        }

        public float Radius
        {
            get { return Internals.SphereCollider_GetRadius(scenePtr, entityId); }
            set { Internals.SphereCollider_SetRadius(scenePtr, entityId, value); }
        }

        public bool IsTrigger
        {
            get { return Internals.SphereCollider_GetIsTrigger(scenePtr, entityId); }
            set { Internals.SphereCollider_SetIsTrigger(scenePtr, entityId, value); }
        }

        public SphereColliderComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }
    }
}
