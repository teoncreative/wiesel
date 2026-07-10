using System;

namespace WieselEngine
{
    public class BoxColliderComponent : Component
    {

        public Vector3f Offset
        {
            get
            {
                return new Vector3f(
                    Internals.BoxCollider_GetOffsetX(entity.ScenePtr, entity.Id),
                    Internals.BoxCollider_GetOffsetY(entity.ScenePtr, entity.Id),
                    Internals.BoxCollider_GetOffsetZ(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.BoxCollider_SetOffsetX(entity.ScenePtr, entity.Id, value.X);
                Internals.BoxCollider_SetOffsetY(entity.ScenePtr, entity.Id, value.Y);
                Internals.BoxCollider_SetOffsetZ(entity.ScenePtr, entity.Id, value.Z);
            }
        }

        public Vector3f HalfExtents
        {
            get
            {
                return new Vector3f(
                    Internals.BoxCollider_GetHalfExtentsX(entity.ScenePtr, entity.Id),
                    Internals.BoxCollider_GetHalfExtentsY(entity.ScenePtr, entity.Id),
                    Internals.BoxCollider_GetHalfExtentsZ(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.BoxCollider_SetHalfExtentsX(entity.ScenePtr, entity.Id, value.X);
                Internals.BoxCollider_SetHalfExtentsY(entity.ScenePtr, entity.Id, value.Y);
                Internals.BoxCollider_SetHalfExtentsZ(entity.ScenePtr, entity.Id, value.Z);
            }
        }

        public bool IsTrigger
        {
            get { return Internals.BoxCollider_GetIsTrigger(entity.ScenePtr, entity.Id); }
            set { Internals.BoxCollider_SetIsTrigger(entity.ScenePtr, entity.Id, value); }
        }

        public BoxColliderComponent(Entity entity) : base(entity) { }
    }
}
