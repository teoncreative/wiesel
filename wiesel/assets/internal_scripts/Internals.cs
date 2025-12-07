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

    }
}