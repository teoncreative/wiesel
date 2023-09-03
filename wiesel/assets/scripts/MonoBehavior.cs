using System;

namespace WieselEngine
{
    public class MonoBehavior
    {
        private ulong behaviorPtr;
        protected ulong b;

        public void OnStart()
        {
            EngineInternal.LogInfo("Start OG!");
        }

        public void OnUpdate(float deltaTime)
        {
            EngineInternal.LogInfo("Update OG!");
        }

        public T GetComponent<T>()
        {
            return (T)EngineInternal.GetComponent(behaviorPtr, typeof(T).Name);
        }

        public void SetHandle(ulong behaviorPtr)
        {
            this.behaviorPtr = behaviorPtr;
        }
    }
}