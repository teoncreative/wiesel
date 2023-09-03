using System;

namespace WieselEngine
{
    public class MonoBehavior
    {
        private ulong behaviorPtr;

        public void OnStart()
        {
        }

        public void OnUpdate(float deltaTime)
        {
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