using System.Runtime.CompilerServices;

namespace WieselEngine
{
    internal class Internals
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
        public static extern Vector3f TransformComponent_GetWorldPosition(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetWorldScale(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_LocalToWorldDirection(ulong scenePtr, ulong entityId, float x, float y, float z);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_WorldToLocalDirection(ulong scenePtr, ulong entityId, float x, float y, float z);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_LocalToWorldPoint(ulong scenePtr, ulong entityId, float x, float y, float z);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_WorldToLocalPoint(ulong scenePtr, ulong entityId, float x, float y, float z);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_Translate(ulong scenePtr, ulong entityId, float x, float y, float z, int space);

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
        public static extern void Animator_Play(ulong scenePtr, ulong entityId, string stateName);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Animator_Stop(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern string Animator_GetCurrentState(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Animator_GetIsPlaying(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Animator_SetIsPlaying(ulong scenePtr, ulong entityId, bool value);


        // SceneManager
        // Synchronous scene loading
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SceneManager_LoadScene(string name, int mode);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SceneManager_LoadScenePath(string path, int mode);

        // Async scene loading (queued for next frame)
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SceneManager_LoadSceneAsync(string name, int mode);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SceneManager_LoadSceneAsyncPath(string path, int mode);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SceneManager_LoadSceneWithLoading(string targetScene, string loadingScene);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float SceneManager_GetLoadProgress();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool SceneManager_IsSceneReady();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SceneManager_ActivateLoadedScene();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SceneManager_UnloadScene(string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int SceneManager_GetLoadedSceneCount();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern ulong SceneManager_GetLoadedScene(int index);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern ulong SceneManager_FindScene(string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Entity SceneManager_MoveEntityToScene(ulong scenePtr, ulong entityId, ulong targetScenePtr, bool moveChildren);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern string Scene_GetName(ulong scenePtr);

        // Prefab
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Entity Prefab_Instantiate(ulong scenePtr, string assetHandle);

        // Time
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Time_GetDeltaTime();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Time_GetTimeScale();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Time_SetTimeScale(float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Time_GetElapsedTime();

        // Scene
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Entity Scene_CreateEntity(ulong scenePtr, string name);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Entity Scene_FindEntity(ulong scenePtr, string name);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Scene_DestroyEntity(ulong scenePtr, ulong entityId);

        // CameraComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Camera_GetProjectionMode(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetProjectionMode(ulong scenePtr, ulong entityId, int mode);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetFOV(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetFOV(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetOrthoSize(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetOrthoSize(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetNearPlane(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetNearPlane(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetFarPlane(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetFarPlane(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Camera_GetEnabled(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetEnabled(ulong scenePtr, ulong entityId, bool v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetBgColorR(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetBgColorG(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetBgColorB(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetBgColorA(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetBgColorR(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetBgColorG(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetBgColorB(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetBgColorA(ulong scenePtr, ulong entityId, float v);

        // SpriteRendererComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool SpriteRenderer_GetFlipX(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteRenderer_SetFlipX(ulong scenePtr, ulong entityId, bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool SpriteRenderer_GetFlipY(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteRenderer_SetFlipY(ulong scenePtr, ulong entityId, bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float SpriteRenderer_GetTintR(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float SpriteRenderer_GetTintG(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float SpriteRenderer_GetTintB(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float SpriteRenderer_GetTintA(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteRenderer_SetTintR(ulong scenePtr, ulong entityId, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteRenderer_SetTintG(ulong scenePtr, ulong entityId, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteRenderer_SetTintB(ulong scenePtr, ulong entityId, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteRenderer_SetTintA(ulong scenePtr, ulong entityId, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int SpriteRenderer_GetSortLayer(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteRenderer_SetSortLayer(ulong scenePtr, ulong entityId, int value);

        // SpriteAnimatorComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteAnimator_Play(ulong scenePtr, ulong entityId, string stateName, bool restart);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteAnimator_Stop(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool SpriteAnimator_GetIsPlaying(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern string SpriteAnimator_GetCurrentState(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int SpriteAnimator_GetCurrentFrame(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteAnimator_SetBool(ulong scenePtr, ulong entityId, string name, bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteAnimator_SetInt(ulong scenePtr, ulong entityId, string name, int value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteAnimator_SetFloat(ulong scenePtr, ulong entityId, string name, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void SpriteAnimator_SetTrigger(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool SpriteAnimator_GetBool(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int SpriteAnimator_GetInt(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float SpriteAnimator_GetFloat(ulong scenePtr, ulong entityId, string name);

        // AudioSourceComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void AudioSource_Play(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void AudioSource_PlayClip(ulong scenePtr, ulong entityId, string clipHandle);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void AudioSource_Stop(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool AudioSource_GetIsPlaying(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float AudioSource_GetVolume(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void AudioSource_SetVolume(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float AudioSource_GetPitch(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void AudioSource_SetPitch(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool AudioSource_GetLoop(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void AudioSource_SetLoop(ulong scenePtr, ulong entityId, bool v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool AudioSource_GetMute(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void AudioSource_SetMute(ulong scenePtr, ulong entityId, bool v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float AudioSource_GetSpatialBlend(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void AudioSource_SetSpatialBlend(ulong scenePtr, ulong entityId, float v);

        // Audio (path-based)
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Audio_PlayPath(string path, int bus, float volume, float pitch, bool loop);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Audio_PlayAtPath(string path, float x, float y, float z, int bus, float volume, float minDist, float maxDist);
        // Audio (clip/handle-based)
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Audio_PlayClip(string handle, int bus, float volume, float pitch, bool loop);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Audio_PlayAtClip(string handle, float x, float y, float z, int bus, float volume, float minDist, float maxDist);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Audio_PlayMusic(string path, float volume);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Audio_PlayMusicClip(string handle, float volume);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Audio_StopMusic();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Audio_SetMasterVolume(float volume);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Audio_GetMasterVolume();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Audio_SetSFXVolume(float volume);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Audio_GetSFXVolume();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Audio_SetMusicVolume(float volume);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Audio_GetMusicVolume();

        // LightDirectComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightDirect_GetColorR(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightDirect_GetColorG(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightDirect_GetColorB(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightDirect_SetColorR(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightDirect_SetColorG(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightDirect_SetColorB(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightDirect_GetAmbient(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightDirect_SetAmbient(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightDirect_GetDiffuse(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightDirect_SetDiffuse(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightDirect_GetSpecular(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightDirect_SetSpecular(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightDirect_GetDensity(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightDirect_SetDensity(ulong scenePtr, ulong entityId, float v);

        // LightPointComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightPoint_GetColorR(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightPoint_GetColorG(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightPoint_GetColorB(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightPoint_SetColorR(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightPoint_SetColorG(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightPoint_SetColorB(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightPoint_GetAmbient(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightPoint_SetAmbient(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightPoint_GetDiffuse(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightPoint_SetDiffuse(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightPoint_GetSpecular(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightPoint_SetSpecular(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightPoint_GetDensity(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightPoint_SetDensity(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightPoint_GetConstant(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightPoint_SetConstant(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightPoint_GetLinear(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightPoint_SetLinear(ulong scenePtr, ulong entityId, float v);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float LightPoint_GetExp(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void LightPoint_SetExp(ulong scenePtr, ulong entityId, float v);

        // Entity components
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Entity_AddComponent(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Entity_RemoveComponent(ulong scenePtr, ulong entityId, string name);

        // Mouse
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Input_GetMouseX();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Input_GetMouseY();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Input_GetMouseButton(int button);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Input_GetMouseButtonDown(int button);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Input_GetMouseButtonUp(int button);

        // Gamepad
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Input_GetGamepadButton(int gamepadIndex, int button);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Input_GetGamepadAxis(int gamepadIndex, int axis);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Input_GetConnectedGamepadCount();

        // Entity tags
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Entity_HasTag(ulong scenePtr, ulong entityId, string tag);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Entity_AddTag(ulong scenePtr, ulong entityId, string tag);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Entity_RemoveTag(ulong scenePtr, ulong entityId, string tag);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Entity[] Scene_FindEntitiesByTag(ulong scenePtr, string tag);

        // Entity validity
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Entity_IsValid(ulong scenePtr, ulong entityId);

        // Child entity access
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Entity_GetChildCount(ulong scenePtr, ulong entityId);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Entity Entity_GetChild(ulong scenePtr, ulong entityId, int index);

        // UIDocument
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void UIDocument_SetInt(ulong scenePtr, ulong entityId, string name, int value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int UIDocument_GetInt(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void UIDocument_SetFloat(ulong scenePtr, ulong entityId, string name, float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float UIDocument_GetFloat(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void UIDocument_SetString(ulong scenePtr, ulong entityId, string name, string value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern string UIDocument_GetString(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void UIDocument_SetBool(ulong scenePtr, ulong entityId, string name, bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool UIDocument_GetBool(ulong scenePtr, ulong entityId, string name);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void UIDocument_SetVisible(ulong scenePtr, ulong entityId, bool visible);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool UIDocument_GetVisible(ulong scenePtr, ulong entityId);

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

        // Settings - Quality
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Settings_GetShadowQuality();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetShadowQuality(int quality);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Settings_GetAnisotropicFiltering();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetAnisotropicFiltering(int value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Settings_GetTextureQuality();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetTextureQuality(int value);

        // Cursor
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Cursor_SetState(string state);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern string Cursor_GetState();

        // Settings - Graphics
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Settings_GetVSync();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetVSync(bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Settings_GetSSAO();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetSSAO(bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Settings_GetBloom();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetBloom(bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Settings_GetMotionBlur();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetMotionBlur(bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Settings_GetShadows();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetShadows(bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Settings_GetRTShadows();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetRTShadows(bool value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern bool Settings_IsRTSupported();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern int Settings_GetAAMode();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetAAMode(int mode);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Settings_GetBloomIntensity();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetBloomIntensity(float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Settings_GetMotionBlurStrength();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetMotionBlurStrength(float value);

        // Settings - Audio
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Settings_GetMasterVolume();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetMasterVolume(float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Settings_GetMusicVolume();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetMusicVolume(float value);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float Settings_GetSFXVolume();
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void Settings_SetSFXVolume(float value);
    }
}