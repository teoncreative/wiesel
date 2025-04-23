using System;

namespace WieselEngine
{
    public class MonoBehavior
    {
        private ulong behaviorPtr;
        private ulong scenePtr;
        private ulong entityId;

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
            return (T)Internals.Behavior_GetComponent(scenePtr, entityId, typeof(T).Name);
        }

        public bool HasComponent<T>()
        {
            return Internals.Behavior_HasComponent(scenePtr, entityId, typeof(T).Name);
        }

    }
}