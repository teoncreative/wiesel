using System.Runtime.CompilerServices;

namespace WieselEngine
{
    public class Internals
    {
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static void Log_Info(string message);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static float Input_GetAxis(string axis);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static bool Input_GetKey(string key);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static void Input_SetCursorMode(ushort cursorMode);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static ushort Input_GetCursorMode();

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static object Behavior_GetComponent(ulong scenePtr, ulong entityId, string name);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static bool Behavior_HasComponent(ulong scenePtr, ulong entityId, string name);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetPositionX(ulong scenePtr, ulong entityId, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetPositionY(ulong scenePtr, ulong entityId, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetPositionZ(ulong scenePtr, ulong entityId, float z);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetPositionX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetPositionY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetPositionZ(ulong scenePtr, ulong entityId);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetRotationX(ulong scenePtr, ulong entityId, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetRotationY(ulong scenePtr, ulong entityId, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetRotationZ(ulong scenePtr, ulong entityId, float z);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetRotationX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetRotationY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetRotationZ(ulong scenePtr, ulong entityId);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetScaleX(ulong scenePtr, ulong entityId, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetScaleY(ulong scenePtr, ulong entityId, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetScaleZ(ulong scenePtr, ulong entityId, float z);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetScaleX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetScaleY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetScaleZ(ulong scenePtr, ulong entityId);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetForward(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetBackward(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetLeft(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetRight(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetUp(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetDown(ulong scenePtr, ulong entityId);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool ModelComponent_GetEnableRendering(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void ModelComponent_SetEnableRendering(ulong scenePtr, ulong entityId, bool value);

        // BoxColliderComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float BoxCollider_GetOffsetX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float BoxCollider_GetOffsetY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float BoxCollider_GetOffsetZ(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void BoxCollider_SetOffsetX(ulong scenePtr, ulong entityId, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void BoxCollider_SetOffsetY(ulong scenePtr, ulong entityId, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void BoxCollider_SetOffsetZ(ulong scenePtr, ulong entityId, float z);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float BoxCollider_GetHalfExtentsX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float BoxCollider_GetHalfExtentsY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float BoxCollider_GetHalfExtentsZ(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void BoxCollider_SetHalfExtentsX(ulong scenePtr, ulong entityId, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void BoxCollider_SetHalfExtentsY(ulong scenePtr, ulong entityId, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void BoxCollider_SetHalfExtentsZ(ulong scenePtr, ulong entityId, float z);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool BoxCollider_GetIsTrigger(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void BoxCollider_SetIsTrigger(ulong scenePtr, ulong entityId, bool value);

        // SphereColliderComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float SphereCollider_GetOffsetX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float SphereCollider_GetOffsetY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float SphereCollider_GetOffsetZ(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SphereCollider_SetOffsetX(ulong scenePtr, ulong entityId, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SphereCollider_SetOffsetY(ulong scenePtr, ulong entityId, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SphereCollider_SetOffsetZ(ulong scenePtr, ulong entityId, float z);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float SphereCollider_GetRadius(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SphereCollider_SetRadius(ulong scenePtr, ulong entityId, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool SphereCollider_GetIsTrigger(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SphereCollider_SetIsTrigger(ulong scenePtr, ulong entityId, bool value);

        // RigidBodyComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int RigidBody_GetType(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_SetType(ulong scenePtr, ulong entityId, int value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetMass(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_SetMass(ulong scenePtr, ulong entityId, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetLinearVelocityX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetLinearVelocityY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetLinearVelocityZ(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_SetLinearVelocity(ulong scenePtr, ulong entityId, float x, float y, float z);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetAngularVelocityX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetAngularVelocityY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetAngularVelocityZ(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_SetAngularVelocity(ulong scenePtr, ulong entityId, float x, float y, float z);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetFriction(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_SetFriction(ulong scenePtr, ulong entityId, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetRestitution(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_SetRestitution(ulong scenePtr, ulong entityId, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetLinearDamping(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_SetLinearDamping(ulong scenePtr, ulong entityId, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RigidBody_GetAngularDamping(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_SetAngularDamping(ulong scenePtr, ulong entityId, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_AddForce(ulong scenePtr, ulong entityId, float x, float y, float z);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_AddImpulse(ulong scenePtr, ulong entityId, float x, float y, float z);

        // Physics queries
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Physics_Raycast(ulong scenePtr,
            float ox, float oy, float oz,
            float dx, float dy, float dz, float maxDist,
            ulong ignoreEntity,
            out ulong hitEntity,
            out float hitPx, out float hitPy, out float hitPz,
            out float hitNx, out float hitNy, out float hitNz,
            out float hitDist);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern ulong[] Physics_OverlapBox(ulong scenePtr, float cx, float cy, float cz, float hx, float hy, float hz);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern ulong[] Physics_OverlapSphere(ulong scenePtr, float cx, float cy, float cz, float radius);

    }
}