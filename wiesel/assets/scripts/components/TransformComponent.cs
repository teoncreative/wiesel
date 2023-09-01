using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System;

namespace WieselEngine
{
    public class TransformComponent
    {
        private uint entityId;
        private ulong scenePtr;
        private HandledVector3f position;

        public Vector3f Position
        {
            get
            {
                EngineInternal.LogInfo(position.GetType().FullName);
                return position;
            }
            set
            {
                position.X = value.X;
                position.Y = value.Y;
                position.Z = value.Z;
            }
        }

        public TransformComponent(uint entityId, ulong scenePtr)
        {
            this.entityId = entityId;
            this.scenePtr = scenePtr;
            this.position = new HandledVector3f(GetPositionX, SetPositionX, GetPositionY, SetPositionY, GetPositionZ, SetPositionZ);
        }

        private float GetPositionX()
        {
            return GetPositionX(entityId, scenePtr);
        }

        private float GetPositionY()
        {
            return GetPositionY(entityId, scenePtr);
        }

        private float GetPositionZ()
        {
            return GetPositionZ(entityId, scenePtr);
        }

        private void SetPositionX(float x)
        {
            SetPositionX(entityId, scenePtr, x);
        }

        private void SetPositionY(float y)
        {
            SetPositionY(entityId, scenePtr, y);
        }

        private void SetPositionZ(float z)
        {
            SetPositionZ(entityId, scenePtr, z);
        }

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetPositionX(uint entityId, ulong scenePtr, float x);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetPositionY(uint entityId, ulong scenePtr, float y);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern void SetPositionZ(uint entityId, ulong scenePtr, float z);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetPositionX(uint entityId, ulong scenePtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetPositionY(uint entityId, ulong scenePtr);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        static extern float GetPositionZ(uint entityId, ulong scenePtr);
    }
}