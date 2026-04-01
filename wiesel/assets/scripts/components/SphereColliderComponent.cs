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
                    Internals.SphereCollider_GetOffsetX(entity.ScenePtr, entity.Id),
                    Internals.SphereCollider_GetOffsetY(entity.ScenePtr, entity.Id),
                    Internals.SphereCollider_GetOffsetZ(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.SphereCollider_SetOffsetX(entity.ScenePtr, entity.Id, value.X);
                Internals.SphereCollider_SetOffsetY(entity.ScenePtr, entity.Id, value.Y);
                Internals.SphereCollider_SetOffsetZ(entity.ScenePtr, entity.Id, value.Z);
            }
        }

        public float Radius
        {
            get { return Internals.SphereCollider_GetRadius(entity.ScenePtr, entity.Id); }
            set { Internals.SphereCollider_SetRadius(entity.ScenePtr, entity.Id, value); }
        }

        public bool IsTrigger
        {
            get { return Internals.SphereCollider_GetIsTrigger(entity.ScenePtr, entity.Id); }
            set { Internals.SphereCollider_SetIsTrigger(entity.ScenePtr, entity.Id, value); }
        }

        public SphereColliderComponent(Entity entity)
        {
            this.entity = entity;
        }
    }
}
