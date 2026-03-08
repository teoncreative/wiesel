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
        public extern static bool Input_GetKeyDown(string key);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static bool Input_GetKeyUp(string key);

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

        // RectangleTransformComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetPositionX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetPositionY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetPositionX(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetPositionY(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetRotation(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetRotation(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetSizeX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetSizeY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetSizeX(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetSizeY(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetScaleX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetScaleY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetScaleX(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetScaleY(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int RectTransform_GetAnchor(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetAnchor(ulong scenePtr, ulong entityId, int v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int RectTransform_GetPivot(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetPivot(ulong scenePtr, ulong entityId, int v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int RectTransform_GetSizeModeX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetSizeModeX(ulong scenePtr, ulong entityId, int v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int RectTransform_GetSizeModeY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetSizeModeY(ulong scenePtr, ulong entityId, int v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetPaddingLeft(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetPaddingTop(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetPaddingRight(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetPaddingBottom(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetPaddingLeft(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetPaddingTop(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetPaddingRight(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void RectTransform_SetPaddingBottom(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetComputedPositionX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetComputedPositionY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetComputedSizeX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float RectTransform_GetComputedSizeY(ulong scenePtr, ulong entityId);

        // CanvasComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Canvas_GetDirection(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Canvas_SetDirection(ulong scenePtr, ulong entityId, int v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Canvas_GetAlignment(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Canvas_SetAlignment(ulong scenePtr, ulong entityId, int v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Canvas_GetSpacing(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Canvas_SetSpacing(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Canvas_GetSortOrder(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Canvas_SetSortOrder(ulong scenePtr, ulong entityId, int v);

        // CanvasRectComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasRect_GetColorR(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasRect_GetColorG(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasRect_GetColorB(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasRect_GetColorA(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasRect_SetColorR(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasRect_SetColorG(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasRect_SetColorB(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasRect_SetColorA(ulong scenePtr, ulong entityId, float v);

        // CanvasImageComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasImage_GetTintR(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasImage_GetTintG(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasImage_GetTintB(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasImage_GetTintA(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasImage_SetTintR(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasImage_SetTintG(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasImage_SetTintB(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasImage_SetTintA(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasImage_GetUVRectX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasImage_GetUVRectY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasImage_GetUVRectZ(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float CanvasImage_GetUVRectW(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasImage_SetUVRectX(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasImage_SetUVRectY(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasImage_SetUVRectZ(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void CanvasImage_SetUVRectW(ulong scenePtr, ulong entityId, float v);

        // TextComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern string Text_GetText(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Text_SetText(ulong scenePtr, ulong entityId, string v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern string Text_GetFontPath(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Text_SetFontPath(ulong scenePtr, ulong entityId, string v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Text_GetFontSize(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Text_SetFontSize(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Text_GetColorR(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Text_GetColorG(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Text_GetColorB(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Text_GetColorA(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Text_SetColorR(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Text_SetColorG(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Text_SetColorB(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Text_SetColorA(ulong scenePtr, ulong entityId, float v);

        // AnimatorComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Animator_SetBool(ulong scenePtr, ulong entityId, string name, bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Animator_SetInt(ulong scenePtr, ulong entityId, string name, int value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Animator_SetFloat(ulong scenePtr, ulong entityId, string name, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Animator_SetTrigger(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Animator_GetBool(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Animator_GetInt(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Animator_GetFloat(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Animator_Play(ulong scenePtr, ulong entityId, string stateName, float blendTime);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern string Animator_GetCurrentState(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Animator_GetIsPlaying(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Animator_SetIsPlaying(ulong scenePtr, ulong entityId, bool value);


        // SceneManager
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SceneManager_LoadScene(string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SceneManager_LoadScenePath(string path);

        // Prefab
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern ulong Prefab_Instantiate(ulong scenePtr, string path);

        // Scene
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern ulong Scene_FindEntity(ulong scenePtr, string name);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Scene_DestroyEntity(ulong scenePtr, ulong entityId);

        // Console
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Console_RegisterCommand(string name, string description, System.Action<string[]> callback);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Console_UnregisterCommand(string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Console_Execute(string commandLine);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Console_LogInfo(string message);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Console_LogWarning(string message);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Console_LogError(string message);
    }
}