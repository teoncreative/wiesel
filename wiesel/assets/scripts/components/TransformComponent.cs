using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System;

namespace WieselEngine
{
    public class TransformComponent
    {
        private ulong behaviorPtr;
        private HandledVector3f position;
        private HandledVector3f rotation;
        private HandledVector3f scale;

        public Vector3f Position
        {
            get
            {
                return position;
            }
            set
            {
                position.X = value.X;
                position.Y = value.Y;
                position.Z = value.Z;
            }
        }

        public TransformComponent(ulong behaviorPtr)
        {
            this.behaviorPtr = behaviorPtr;
            this.position = new HandledVector3f(GetPositionX, SetPositionX, GetPositionY, SetPositionY, GetPositionZ, SetPositionZ);
            this.rotation = new HandledVector3f(GetRotationX, SetRotationX, GetRotationY, SetRotationY, GetRotationZ, SetRotationZ);
            this.scale = new HandledVector3f(GetScaleX, SetScaleX, GetScaleY, SetScaleY, GetScaleZ, SetScaleZ);
        }

        private float GetPositionX()
        {
            return GetPositionX(behaviorPtr);
        }

        private float GetPositionY()
        {
            return GetPositionY(behaviorPtr);
        }

        private float GetPositionZ()
        {
            return GetPositionZ(behaviorPtr);
        }

        private void SetPositionX(float x)
        {
            SetPositionX(behaviorPtr, x);
        }

        private void SetPositionY(float y)
        {
            SetPositionY(behaviorPtr, y);
        }

        private void SetPositionZ(float z)
        {
            SetPositionZ(behaviorPtr, z);
        }

        private float GetRotationX()
        {
            return GetRotationX(behaviorPtr);
        }

        private float GetRotationY()
        {
            return GetRotationY(behaviorPtr);
        }

        private float GetRotationZ()
        {
            return GetRotationZ(behaviorPtr);
        }

        private void SetRotationX(float x)
        {
            SetRotationX(behaviorPtr, x);
        }

        private void SetRotationY(float y)
        {
            SetRotationY(behaviorPtr, y);
        }

        private void SetRotationZ(float z)
        {
            SetRotationZ(behaviorPtr, z);
        }

        private float GetScaleX()
        {
            return GetScaleX(behaviorPtr);
        }

        private float GetScaleY()
        {
            return GetScaleY(behaviorPtr);
        }

        private float GetScaleZ()
        {
            return GetScaleZ(behaviorPtr);
        }

        private void SetScaleX(float x)
        {
            SetScaleX(behaviorPtr, x);
        }

        private void SetScaleY(float y)
        {
            SetScaleY(behaviorPtr, y);
        }

        private void SetScaleZ(float z)
        {
            SetScaleZ(behaviorPtr, z);
        }

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetPositionX(ulong behaviorPtr, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetPositionY(ulong behaviorPtr, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetPositionZ(ulong behaviorPtr, float z);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetPositionX(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetPositionY(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetPositionZ(ulong behaviorPtr);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetRotationX(ulong behaviorPtr, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetRotationY(ulong behaviorPtr, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetRotationZ(ulong behaviorPtr, float z);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetRotationX(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetRotationY(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetRotationZ(ulong behaviorPtr);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetScaleX(ulong behaviorPtr, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetScaleY(ulong behaviorPtr, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetScaleZ(ulong behaviorPtr, float z);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetScaleX(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetScaleY(ulong behaviorPtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetScaleZ(ulong behaviorPtr);


    }
}