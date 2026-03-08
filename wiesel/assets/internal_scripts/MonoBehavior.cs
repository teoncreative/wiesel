using System;

namespace WieselEngine
{
    public class MonoBehavior
    {
        private ulong behaviorPtr;
        private ulong scenePtr;
        private ulong entityId;

        protected ulong ScenePtr { get { return scenePtr; } }
        protected ulong EntityId { get { return entityId; } }

        public Entity Entity { get { return new Entity(scenePtr, entityId); } }

        public static implicit operator Entity(MonoBehavior behavior)
        {
            return behavior.Entity;
        }

        public Entity GetEntity(ulong otherEntityId)
        {
            return new Entity(scenePtr, otherEntityId);
        }

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

        public virtual void OnTriggerEnter(ulong otherEntityId)
        {
        }

        public virtual void OnTriggerStay(ulong otherEntityId)
        {
        }

        public virtual void OnTriggerExit(ulong otherEntityId)
        {
        }

        public virtual void OnCollisionEnter(ulong otherEntityId)
        {
        }

        public virtual void OnCollisionStay(ulong otherEntityId)
        {
        }

        public virtual void OnCollisionExit(ulong otherEntityId)
        {
        }

        public T GetComponent<T>()
        {
            return (T)Internals.Behavior_GetComponent(scenePtr, entityId, typeof(T).Name);
        }

        public bool HasComponent<T>()
        {
            return Internals.Behavior_HasComponent(scenePtr, entityId, typeof(T).Name);
        }

        public Entity FindEntity(string name)
        {
            ulong id = Internals.Scene_FindEntity(scenePtr, name);
            if (id == ulong.MaxValue) return null;
            return new Entity(scenePtr, id);
        }

        public void DestroyEntity(Entity entity)
        {
            Internals.Scene_DestroyEntity(scenePtr, entity.Id);
        }

        public void Destroy()
        {
            Internals.Scene_DestroyEntity(scenePtr, entityId);
        }

    }
}
