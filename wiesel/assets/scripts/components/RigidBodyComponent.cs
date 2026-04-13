namespace WieselEngine
{
    public enum RigidBodyType
    {
        Static = 0,
        Kinematic = 1,
        Dynamic = 2
    }

    public class RigidBodyComponent : Component
    {

        public RigidBodyComponent(Entity entity)
        {
            this.entity = entity;
        }

        public RigidBodyType Type
        {
            get { return (RigidBodyType)Internals.RigidBody_GetType(entity.ScenePtr, entity.Id); }
            set { Internals.RigidBody_SetType(entity.ScenePtr, entity.Id, (int)value); }
        }

        public float Mass
        {
            get { return Internals.RigidBody_GetMass(entity.ScenePtr, entity.Id); }
            set { Internals.RigidBody_SetMass(entity.ScenePtr, entity.Id, value); }
        }

        public Vector3f LinearVelocity
        {
            get
            {
                return new Vector3f(
                    Internals.RigidBody_GetLinearVelocityX(entity.ScenePtr, entity.Id),
                    Internals.RigidBody_GetLinearVelocityY(entity.ScenePtr, entity.Id),
                    Internals.RigidBody_GetLinearVelocityZ(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.RigidBody_SetLinearVelocity(entity.ScenePtr, entity.Id, value.X, value.Y, value.Z);
            }
        }

        public Vector3f AngularVelocity
        {
            get
            {
                return new Vector3f(
                    Internals.RigidBody_GetAngularVelocityX(entity.ScenePtr, entity.Id),
                    Internals.RigidBody_GetAngularVelocityY(entity.ScenePtr, entity.Id),
                    Internals.RigidBody_GetAngularVelocityZ(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.RigidBody_SetAngularVelocity(entity.ScenePtr, entity.Id, value.X, value.Y, value.Z);
            }
        }

        public float Friction
        {
            get { return Internals.RigidBody_GetFriction(entity.ScenePtr, entity.Id); }
            set { Internals.RigidBody_SetFriction(entity.ScenePtr, entity.Id, value); }
        }

        public float Restitution
        {
            get { return Internals.RigidBody_GetRestitution(entity.ScenePtr, entity.Id); }
            set { Internals.RigidBody_SetRestitution(entity.ScenePtr, entity.Id, value); }
        }

        public float LinearDamping
        {
            get { return Internals.RigidBody_GetLinearDamping(entity.ScenePtr, entity.Id); }
            set { Internals.RigidBody_SetLinearDamping(entity.ScenePtr, entity.Id, value); }
        }

        public float AngularDamping
        {
            get { return Internals.RigidBody_GetAngularDamping(entity.ScenePtr, entity.Id); }
            set { Internals.RigidBody_SetAngularDamping(entity.ScenePtr, entity.Id, value); }
        }

        public void AddForce(Vector3f force)
        {
            Internals.RigidBody_AddForce(entity.ScenePtr, entity.Id, force.X, force.Y, force.Z);
        }

        public void AddImpulse(Vector3f impulse)
        {
            Internals.RigidBody_AddImpulse(entity.ScenePtr, entity.Id, impulse.X, impulse.Y, impulse.Z);
        }
    }
}
