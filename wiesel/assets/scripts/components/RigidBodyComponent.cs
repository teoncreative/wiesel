namespace WieselEngine
{
    public class RigidBodyComponent : Component
    {

        public RigidBodyComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        public int Type
        {
            get { return Internals.RigidBody_GetType(scenePtr, entityId); }
            set { Internals.RigidBody_SetType(scenePtr, entityId, value); }
        }

        public float Mass
        {
            get { return Internals.RigidBody_GetMass(scenePtr, entityId); }
            set { Internals.RigidBody_SetMass(scenePtr, entityId, value); }
        }

        public Vector3f LinearVelocity
        {
            get
            {
                return new Vector3f(
                    Internals.RigidBody_GetLinearVelocityX(scenePtr, entityId),
                    Internals.RigidBody_GetLinearVelocityY(scenePtr, entityId),
                    Internals.RigidBody_GetLinearVelocityZ(scenePtr, entityId));
            }
            set
            {
                Internals.RigidBody_SetLinearVelocity(scenePtr, entityId, value.X, value.Y, value.Z);
            }
        }

        public Vector3f AngularVelocity
        {
            get
            {
                return new Vector3f(
                    Internals.RigidBody_GetAngularVelocityX(scenePtr, entityId),
                    Internals.RigidBody_GetAngularVelocityY(scenePtr, entityId),
                    Internals.RigidBody_GetAngularVelocityZ(scenePtr, entityId));
            }
            set
            {
                Internals.RigidBody_SetAngularVelocity(scenePtr, entityId, value.X, value.Y, value.Z);
            }
        }

        public float Friction
        {
            get { return Internals.RigidBody_GetFriction(scenePtr, entityId); }
            set { Internals.RigidBody_SetFriction(scenePtr, entityId, value); }
        }

        public float Restitution
        {
            get { return Internals.RigidBody_GetRestitution(scenePtr, entityId); }
            set { Internals.RigidBody_SetRestitution(scenePtr, entityId, value); }
        }

        public float LinearDamping
        {
            get { return Internals.RigidBody_GetLinearDamping(scenePtr, entityId); }
            set { Internals.RigidBody_SetLinearDamping(scenePtr, entityId, value); }
        }

        public float AngularDamping
        {
            get { return Internals.RigidBody_GetAngularDamping(scenePtr, entityId); }
            set { Internals.RigidBody_SetAngularDamping(scenePtr, entityId, value); }
        }

        public void AddForce(Vector3f force)
        {
            Internals.RigidBody_AddForce(scenePtr, entityId, force.X, force.Y, force.Z);
        }

        public void AddImpulse(Vector3f impulse)
        {
            Internals.RigidBody_AddImpulse(scenePtr, entityId, impulse.X, impulse.Y, impulse.Z);
        }
    }
}
