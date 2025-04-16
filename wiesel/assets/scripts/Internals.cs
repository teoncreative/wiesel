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
        public extern static object Behavior_GetComponent(ulong behaviorPtr, string name);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public extern static bool Behavior_HasComponent(ulong behaviorPtr, string name);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetPositionX(ulong behaviorPtr, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetPositionY(ulong behaviorPtr, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetPositionZ(ulong behaviorPtr, float z);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetPositionX(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetPositionY(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetPositionZ(ulong behaviorPtr);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetRotationX(ulong behaviorPtr, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetRotationY(ulong behaviorPtr, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetRotationZ(ulong behaviorPtr, float z);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetRotationX(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetRotationY(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetRotationZ(ulong behaviorPtr);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetScaleX(ulong behaviorPtr, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetScaleY(ulong behaviorPtr, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void TransformComponent_SetScaleZ(ulong behaviorPtr, float z);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetScaleX(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetScaleY(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern float TransformComponent_GetScaleZ(ulong behaviorPtr);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetForward(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetBackward(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetLeft(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetRight(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetUp(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Vector3f TransformComponent_GetDown(ulong behaviorPtr);

    }
}