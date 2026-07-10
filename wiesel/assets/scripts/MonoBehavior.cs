using System;

namespace WieselEngine
{
    /// <summary>
    /// Base class for all C# scripts attached to entities.
    /// Override virtual methods to respond to lifecycle events, input, physics, and UI.
    /// </summary>
    public class MonoBehavior
    {
        private ulong behaviorPtr;

        /// <summary>The entity this script is attached to. Set by the engine.</summary>
        public Entity Entity;

        public static implicit operator Entity(MonoBehavior behavior)
        {
            return behavior.Entity;
        }

        // -- Lifecycle --

        /// <summary>Called once when the script is first activated, before the first OnUpdate.</summary>
        public virtual void OnStart()
        {
        }

        /// <summary>Called every frame with the time in seconds since the last frame.</summary>
        public virtual void OnUpdate(float deltaTime)
        {
        }

        /// <summary>Called when the script is disabled or the entity is about to be removed.</summary>
        public virtual void OnDisable()
        {
        }

        /// <summary>Called when the entity is destroyed. Always called after OnDisable.</summary>
        public virtual void OnDestroy()
        {
        }

        // -- Input --

        /// <summary>Called when a key is pressed. Return true to consume the event.</summary>
        public virtual bool OnKeyPressed(KeyCode keyCode, bool repeat)
        {
            return false;
        }

        /// <summary>Called when a key is released. Return true to consume the event.</summary>
        public virtual bool OnKeyReleased(KeyCode keyCode)
        {
            return false;
        }

        /// <summary>Called when the mouse moves. Return true to consume the event.</summary>
        public virtual bool OnMouseMoved(float x, float y, CursorMode cursorMode)
        {
            return false;
        }

        // -- Physics: Triggers --

        /// <summary>Called once when this entity's trigger collider first overlaps another.</summary>
        public virtual void OnTriggerEnter(Entity other)
        {
        }

        /// <summary>Called every physics step while this entity's trigger overlaps another.</summary>
        public virtual void OnTriggerStay(Entity other)
        {
        }

        /// <summary>Called once when this entity's trigger stops overlapping another.</summary>
        public virtual void OnTriggerExit(Entity other)
        {
        }

        // -- Physics: Collisions --

        /// <summary>Called once when this entity first makes contact with another.</summary>
        public virtual void OnCollisionEnter(Entity other)
        {
        }

        /// <summary>Called every physics step while this entity is in contact with another.</summary>
        public virtual void OnCollisionStay(Entity other)
        {
        }

        /// <summary>Called once when this entity stops contacting another.</summary>
        public virtual void OnCollisionExit(Entity other)
        {
        }

        // -- UI Pointer Events (requires InteractableComponent) --

        /// <summary>Called when the entity is clicked. Return true to consume.</summary>
        public virtual bool OnPointerClick(float x, float y)
        {
            return false;
        }

        /// <summary>Called when a pointer button is pressed on this entity.</summary>
        public virtual bool OnPointerDown(float x, float y)
        {
            return false;
        }

        /// <summary>Called when a pointer button is released on this entity.</summary>
        public virtual bool OnPointerUp(float x, float y)
        {
            return false;
        }

        /// <summary>Called when the pointer enters this entity's bounds.</summary>
        public virtual void OnPointerEnter()
        {
        }

        /// <summary>Called when the pointer leaves this entity's bounds.</summary>
        public virtual void OnPointerExit()
        {
        }

        // -- RmlUi Data Binding (requires UIDocumentComponent on same entity) --

        /// <summary>
        /// Called when the UI changes a TwoWay data variable
        /// (e.g. user types in an input field bound with data-value).
        /// </summary>
        public virtual void OnUIDataChanged(string variableName)
        {
        }

        /// <summary>
        /// Called when a UI event fires (e.g. data-event-click="eventName" in RML).
        /// Event names must be declared in the .rml asset properties.
        /// </summary>
        public virtual void OnUIEvent(string eventName)
        {
        }

        // -- Network Callbacks --

        /// <summary>Called on all scripts when a client connects to the server (server-side).</summary>
        public virtual void OnClientConnected(ulong sessionId)
        {
        }

        /// <summary>Called on all scripts when a client disconnects from the server (server-side).</summary>
        public virtual void OnClientDisconnected(ulong sessionId)
        {
        }

        /// <summary>Called on all scripts when this client connects to a server (client-side).</summary>
        public virtual void OnConnectedToServer()
        {
        }

        /// <summary>Called on all scripts when this client disconnects from a server (client-side).</summary>
        public virtual void OnDisconnectedFromServer()
        {
        }

        // -- Network RPCs --

        /// <summary>Called on the server when a client invokes SendServerRpc on this entity.</summary>
        public virtual void OnServerRpc(string rpcName)
        {
        }

        /// <summary>Called on clients when the server invokes SendClientRpc on this entity.</summary>
        public virtual void OnClientRpc(string rpcName)
        {
        }

        // -- Synced Variables --

        /// <summary>Called when a NetworkVariable changes on this entity (receiving side).</summary>
        public virtual void OnSyncVarChanged(string varName)
        {
        }

        /// <summary>Call an RPC on the server. The server's OnServerRpc will be invoked.</summary>
        public void SendServerRpc(string rpcName, params object[] args)
        {
            Internals.Network_SendServerRpc(Entity.ScenePtr, Entity.Id, rpcName, args);
        }

        /// <summary>Call an RPC on all clients. Each client's OnClientRpc will be invoked.</summary>
        public void SendClientRpc(string rpcName, params object[] args)
        {
            Internals.Network_SendClientRpc(Entity.ScenePtr, Entity.Id, rpcName, args);
        }

        // -- Component access --

        /// <summary>Get a component of type T from this entity.</summary>
        public T GetComponent<T>()
        {
            return (T)Internals.Behavior_GetComponent(Entity.ScenePtr, Entity.Id, typeof(T).Name);
        }

        /// <summary>Check if this entity has a component of type T.</summary>
        public bool HasComponent<T>()
        {
            return Internals.Behavior_HasComponent(Entity.ScenePtr, Entity.Id, typeof(T).Name);
        }

        /// <summary>Add a component of type T to this entity at runtime.</summary>
        public void AddComponent<T>()
        {
            Internals.Entity_AddComponent(Entity.ScenePtr, Entity.Id, typeof(T).Name);
        }

        /// <summary>Remove a component of type T from this entity.</summary>
        public void RemoveComponent<T>()
        {
            Internals.Entity_RemoveComponent(Entity.ScenePtr, Entity.Id, typeof(T).Name);
        }

        // -- Scene operations --

        /// <summary>Create a new empty entity in the current scene.</summary>
        public Entity CreateEntity(string name)
        {
            return Internals.Scene_CreateEntity(Entity.ScenePtr, name);
        }

        /// <summary>Find an entity by name. Returns null if not found.</summary>
        public Entity FindEntity(string name)
        {
            return Internals.Scene_FindEntity(Entity.ScenePtr, name);
        }

        /// <summary>Queue an entity for destruction at end of frame.</summary>
        public void DestroyEntity(Entity target)
        {
            Internals.Scene_DestroyEntity(Entity.ScenePtr, target.Id);
        }

        /// <summary>Queue this entity for destruction at end of frame.</summary>
        public void Destroy()
        {
            Internals.Scene_DestroyEntity(Entity.ScenePtr, Entity.Id);
        }

        /// <summary>Find all entities with the given tag. Returns empty array if none found.</summary>
        public Entity[] FindEntitiesByTag(string tag)
        {
            Entity[] result = Internals.Scene_FindEntitiesByTag(Entity.ScenePtr, tag);
            if (result == null) return new Entity[0];
            return result;
        }

        /// <summary>
        /// Move this entity (and optionally its children) to another scene.
        /// This entity handle becomes invalid after the move. Returns the new Entity in the target scene.
        /// </summary>
        public Entity MoveToScene(Scene targetScene, bool moveChildren = true)
        {
            return SceneManager.MoveEntityToScene(Entity, targetScene, moveChildren);
        }

    }
}
