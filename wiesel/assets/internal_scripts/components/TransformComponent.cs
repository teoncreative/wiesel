using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System;

namespace WieselEngine
{
    public class TransformComponent
    {
        private ulong scenePtr;
        private ulong entityId;
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

        public TransformComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
            this.position = new HandledVector3f(GetPositionX, SetPositionX, GetPositionY, SetPositionY, GetPositionZ, SetPositionZ);
            this.rotation = new HandledVector3f(GetRotationX, SetRotationX, GetRotationY, SetRotationY, GetRotationZ, SetRotationZ);
            this.scale = new HandledVector3f(GetScaleX, SetScaleX, GetScaleY, SetScaleY, GetScaleZ, SetScaleZ);
        }

        public Vector3f GetForward() {
            return (Vector3f) Internals.TransformComponent_GetForward(scenePtr, entityId);
        }

        public Vector3f GetBackward() {
            return (Vector3f) Internals.TransformComponent_GetBackward(scenePtr, entityId);
        }

        public Vector3f GetLeft() {
            return (Vector3f) Internals.TransformComponent_GetLeft(scenePtr, entityId);
        }

        public Vector3f GetRight() {
            return (Vector3f) Internals.TransformComponent_GetRight(scenePtr, entityId);
        }

        public Vector3f GetUp() {
            return (Vector3f) Internals.TransformComponent_GetUp(scenePtr, entityId);
        }

        public Vector3f GetDown() {
            return (Vector3f) Internals.TransformComponent_GetDown(scenePtr, entityId);
        }

        private float GetPositionX()
        {
            return Internals.TransformComponent_GetPositionX(scenePtr, entityId);
        }

        private float GetPositionY()
        {
            return Internals.TransformComponent_GetPositionY(scenePtr, entityId);
        }

        private float GetPositionZ()
        {
            return Internals.TransformComponent_GetPositionZ(scenePtr, entityId);
        }

        private void SetPositionX(float x)
        {
            Internals.TransformComponent_SetPositionX(scenePtr, entityId, x);
        }

        private void SetPositionY(float y)
        {
            Internals.TransformComponent_SetPositionY(scenePtr, entityId, y);
        }

        private void SetPositionZ(float z)
        {
            Internals.TransformComponent_SetPositionZ(scenePtr, entityId, z);
        }

        private float GetRotationX()
        {
            return Internals.TransformComponent_GetRotationX(scenePtr, entityId);
        }

        private float GetRotationY()
        {
            return Internals.TransformComponent_GetRotationY(scenePtr, entityId);
        }

        private float GetRotationZ()
        {
            return Internals.TransformComponent_GetRotationZ(scenePtr, entityId);
        }

        private void SetRotationX(float x)
        {
            Internals.TransformComponent_SetRotationX(scenePtr, entityId, x);
        }

        private void SetRotationY(float y)
        {
            Internals.TransformComponent_SetRotationY(scenePtr, entityId, y);
        }

        private void SetRotationZ(float z)
        {
            Internals.TransformComponent_SetRotationZ(scenePtr, entityId, z);
        }

        private float GetScaleX()
        {
            return Internals.TransformComponent_GetScaleX(scenePtr, entityId);
        }

        private float GetScaleY()
        {
            return Internals.TransformComponent_GetScaleY(scenePtr, entityId);
        }

        private float GetScaleZ()
        {
            return Internals.TransformComponent_GetScaleZ(scenePtr, entityId);
        }

        private void SetScaleX(float x)
        {
            Internals.TransformComponent_SetScaleX(scenePtr, entityId, x);
        }

        private void SetScaleY(float y)
        {
            Internals.TransformComponent_SetScaleY(scenePtr, entityId, y);
        }

        private void SetScaleZ(float z)
        {
            Internals.TransformComponent_SetScaleZ(scenePtr, entityId, z);
        }
    }
}