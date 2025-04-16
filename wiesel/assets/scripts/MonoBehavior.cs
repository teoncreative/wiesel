using System;

namespace WieselEngine
{
    public class MonoBehavior
    {
        private ulong behaviorPtr;

        public virtual void OnStart()
        {
        }

        public virtual void OnUpdate(float deltaTime)
        {
        }

        public virtual bool OnKeyPressed(KeyCode keyCode, bool repeat)
        {
            return false;
        }

        public virtual bool OnKeyReleased(KeyCode keyCode)
        {
            return false;
        }

        public virtual bool OnMouseMoved(float x, float y, CursorMode cursorMode)
        {
            return false;
        }

        public T GetComponent<T>()
        {
            return (T)Internals.Behavior_GetComponent(behaviorPtr, typeof(T).Name);
        }

        public bool HasComponent<T>()
        {
            return Internals.Behavior_HasComponent(behaviorPtr, typeof(T).Name);
        }

        public void SetHandle(ulong behaviorPtr)
        {
            this.behaviorPtr = behaviorPtr;
        }
    }
}