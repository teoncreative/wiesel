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

        public Vector3f Rotation
        {
            get
            {
                return rotation;
            }
            set
            {
                rotation.X = value.X;
                rotation.Y = value.Y;
                rotation.Z = value.Z;
            }
        }

        public Vector3f Scale
        {
            get
            {
                return scale;
            }
            set
            {
                scale.X = value.X;
                scale.Y = value.Y;
                scale.Z = value.Z;
            }
        }

        public TransformComponent(ulong behaviorPtr)
        {
            this.behaviorPtr = behaviorPtr;
            this.position = new HandledVector3f(GetPositionX, SetPositionX, GetPositionY, SetPositionY, GetPositionZ, SetPositionZ);
            this.rotation = new HandledVector3f(GetRotationX, SetRotationX, GetRotationY, SetRotationY, GetRotationZ, SetRotationZ);
            this.scale = new HandledVector3f(GetScaleX, SetScaleX, GetScaleY, SetScaleY, GetScaleZ, SetScaleZ);
        }

        public Vector3f GetForward() {
            return (Vector3f) Internals.TransformComponent_GetForward(behaviorPtr);
        }

        public Vector3f GetBackward() {
            return (Vector3f) Internals.TransformComponent_GetBackward(behaviorPtr);
        }

        public Vector3f GetLeft() {
            return (Vector3f) Internals.TransformComponent_GetLeft(behaviorPtr);
        }

        public Vector3f GetRight() {
            return (Vector3f) Internals.TransformComponent_GetRight(behaviorPtr);
        }

        public Vector3f GetUp() {
            return (Vector3f) Internals.TransformComponent_GetUp(behaviorPtr);
        }

        public Vector3f GetDown() {
            return (Vector3f) Internals.TransformComponent_GetDown(behaviorPtr);
        }

        private float GetPositionX()
        {
            return Internals.TransformComponent_GetPositionX(behaviorPtr);
        }

        private float GetPositionY()
        {
            return Internals.TransformComponent_GetPositionY(behaviorPtr);
        }

        private float GetPositionZ()
        {
            return Internals.TransformComponent_GetPositionZ(behaviorPtr);
        }

        private void SetPositionX(float x)
        {
            Internals.TransformComponent_SetPositionX(behaviorPtr, x);
        }

        private void SetPositionY(float y)
        {
            Internals.TransformComponent_SetPositionY(behaviorPtr, y);
        }

        private void SetPositionZ(float z)
        {
            Internals.TransformComponent_SetPositionZ(behaviorPtr, z);
        }

        private float GetRotationX()
        {
            return Internals.TransformComponent_GetRotationX(behaviorPtr);
        }

        private float GetRotationY()
        {
            return Internals.TransformComponent_GetRotationY(behaviorPtr);
        }

        private float GetRotationZ()
        {
            return Internals.TransformComponent_GetRotationZ(behaviorPtr);
        }

        private void SetRotationX(float x)
        {
            Internals.TransformComponent_SetRotationX(behaviorPtr, x);
        }

        private void SetRotationY(float y)
        {
            Internals.TransformComponent_SetRotationY(behaviorPtr, y);
        }

        private void SetRotationZ(float z)
        {
            Internals.TransformComponent_SetRotationZ(behaviorPtr, z);
        }

        private float GetScaleX()
        {
            return Internals.TransformComponent_GetScaleX(behaviorPtr);
        }

        private float GetScaleY()
        {
            return Internals.TransformComponent_GetScaleY(behaviorPtr);
        }

        private float GetScaleZ()
        {
            return Internals.TransformComponent_GetScaleZ(behaviorPtr);
        }

        private void SetScaleX(float x)
        {
            Internals.TransformComponent_SetScaleX(behaviorPtr, x);
        }

        private void SetScaleY(float y)
        {
            Internals.TransformComponent_SetScaleY(behaviorPtr, y);
        }

        private void SetScaleZ(float z)
        {
            Internals.TransformComponent_SetScaleZ(behaviorPtr, z);
        }
    }
}