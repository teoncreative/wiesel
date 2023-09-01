using System;

namespace WieselEngine
{
    public class MonoBehavior
    {
        private uint entityId;
        private ulong scenePtr;

        public void Start()
        {
        }

        public void Update()
        {
        }

        public T GetComponent<T>()
        {
            return (T)EngineInternal.GetComponent(this, typeof(T).Name);
        }

        public void SetHandle(uint entityId, ulong scenePtr)
        {
            this.entityId = entityId;
            this.scenePtr = scenePtr;
            EngineInternal.LogInfo($"MonoBehavior {entityId} {scenePtr}");
        }
    }
}