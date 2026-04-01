using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System;

namespace WieselEngine
{
    public enum Space
    {
        Local = 0,
        World = 1
    }

    public class TransformComponent : Component
    {
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

        public TransformComponent(Entity entity)
        {
            this.entity = entity;
            this.position = new HandledVector3f(GetPositionX, SetPositionX, GetPositionY, SetPositionY, GetPositionZ, SetPositionZ);
            this.rotation = new HandledVector3f(GetRotationX, SetRotationX, GetRotationY, SetRotationY, GetRotationZ, SetRotationZ);
            this.scale = new HandledVector3f(GetScaleX, SetScaleX, GetScaleY, SetScaleY, GetScaleZ, SetScaleZ);
        }

        public Vector3f GetForward() {
            return (Vector3f) Internals.TransformComponent_GetForward(entity.ScenePtr, entity.Id);
        }

        public Vector3f GetBackward() {
            return (Vector3f) Internals.TransformComponent_GetBackward(entity.ScenePtr, entity.Id);
        }

        public Vector3f GetLeft() {
            return (Vector3f) Internals.TransformComponent_GetLeft(entity.ScenePtr, entity.Id);
        }

        public Vector3f GetRight() {
            return (Vector3f) Internals.TransformComponent_GetRight(entity.ScenePtr, entity.Id);
        }

        public Vector3f GetUp() {
            return (Vector3f) Internals.TransformComponent_GetUp(entity.ScenePtr, entity.Id);
        }

        public Vector3f GetDown() {
            return (Vector3f) Internals.TransformComponent_GetDown(entity.ScenePtr, entity.Id);
        }

        public Vector3f GetWorldPosition() {
            return (Vector3f) Internals.TransformComponent_GetWorldPosition(entity.ScenePtr, entity.Id);
        }

        public Vector3f GetWorldScale() {
            return (Vector3f) Internals.TransformComponent_GetWorldScale(entity.ScenePtr, entity.Id);
        }

        public Vector3f LocalToWorldDirection(Vector3f dir) {
            return (Vector3f) Internals.TransformComponent_LocalToWorldDirection(entity.ScenePtr, entity.Id, dir.X, dir.Y, dir.Z);
        }

        public Vector3f WorldToLocalDirection(Vector3f dir) {
            return (Vector3f) Internals.TransformComponent_WorldToLocalDirection(entity.ScenePtr, entity.Id, dir.X, dir.Y, dir.Z);
        }

        public Vector3f LocalToWorldPoint(Vector3f point) {
            return (Vector3f) Internals.TransformComponent_LocalToWorldPoint(entity.ScenePtr, entity.Id, point.X, point.Y, point.Z);
        }

        public Vector3f WorldToLocalPoint(Vector3f point) {
            return (Vector3f) Internals.TransformComponent_WorldToLocalPoint(entity.ScenePtr, entity.Id, point.X, point.Y, point.Z);
        }

        public void Translate(Vector3f delta, Space space = Space.Local) {
            Internals.TransformComponent_Translate(entity.ScenePtr, entity.Id, delta.X, delta.Y, delta.Z, (int)space);
        }

        public void Translate(float x, float y, float z, Space space = Space.Local) {
            Internals.TransformComponent_Translate(entity.ScenePtr, entity.Id, x, y, z, (int)space);
        }

        private float GetPositionX()
        {
            return Internals.TransformComponent_GetPositionX(entity.ScenePtr, entity.Id);
        }

        private float GetPositionY()
        {
            return Internals.TransformComponent_GetPositionY(entity.ScenePtr, entity.Id);
        }

        private float GetPositionZ()
        {
            return Internals.TransformComponent_GetPositionZ(entity.ScenePtr, entity.Id);
        }

        private void SetPositionX(float x)
        {
            Internals.TransformComponent_SetPositionX(entity.ScenePtr, entity.Id, x);
        }

        private void SetPositionY(float y)
        {
            Internals.TransformComponent_SetPositionY(entity.ScenePtr, entity.Id, y);
        }

        private void SetPositionZ(float z)
        {
            Internals.TransformComponent_SetPositionZ(entity.ScenePtr, entity.Id, z);
        }

        private float GetRotationX()
        {
            return Internals.TransformComponent_GetRotationX(entity.ScenePtr, entity.Id);
        }

        private float GetRotationY()
        {
            return Internals.TransformComponent_GetRotationY(entity.ScenePtr, entity.Id);
        }

        private float GetRotationZ()
        {
            return Internals.TransformComponent_GetRotationZ(entity.ScenePtr, entity.Id);
        }

        private void SetRotationX(float x)
        {
            Internals.TransformComponent_SetRotationX(entity.ScenePtr, entity.Id, x);
        }

        private void SetRotationY(float y)
        {
            Internals.TransformComponent_SetRotationY(entity.ScenePtr, entity.Id, y);
        }

        private void SetRotationZ(float z)
        {
            Internals.TransformComponent_SetRotationZ(entity.ScenePtr, entity.Id, z);
        }

        private float GetScaleX()
        {
            return Internals.TransformComponent_GetScaleX(entity.ScenePtr, entity.Id);
        }

        private float GetScaleY()
        {
            return Internals.TransformComponent_GetScaleY(entity.ScenePtr, entity.Id);
        }

        private float GetScaleZ()
        {
            return Internals.TransformComponent_GetScaleZ(entity.ScenePtr, entity.Id);
        }

        private void SetScaleX(float x)
        {
            Internals.TransformComponent_SetScaleX(entity.ScenePtr, entity.Id, x);
        }

        private void SetScaleY(float y)
        {
            Internals.TransformComponent_SetScaleY(entity.ScenePtr, entity.Id, y);
        }

        private void SetScaleZ(float z)
        {
            Internals.TransformComponent_SetScaleZ(entity.ScenePtr, entity.Id, z);
        }
    }
}