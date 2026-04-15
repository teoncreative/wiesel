using System;

namespace WieselEngine
{
    public class NetworkVariable<T>
    {
        private T value;
        private string name;
        private MonoBehavior owner;
        private bool bound;

        public event Action<T, T> OnValueChanged;

        public NetworkVariable(T defaultValue = default)
        {
            value = defaultValue;
        }

        public T Value
        {
            get { return value; }
            set
            {
                T old = this.value;
                this.value = value;
                if (bound && owner != null)
                {
                    Internals.Network_SetSyncVar(
                        owner.Entity.ScenePtr, owner.Entity.Id,
                        name, (object)value);
                }
                OnValueChanged?.Invoke(old, value);
            }
        }

        // Called by the engine during script initialization to bind this
        // variable to the sync var system using the field name.
        internal void Bind(MonoBehavior owner, string fieldName)
        {
            this.owner = owner;
            this.name = fieldName;
            this.bound = true;
        }

        // Called by the engine when a remote update is received.
        internal void SetFromNetwork(T newValue)
        {
            T old = value;
            value = newValue;
            OnValueChanged?.Invoke(old, newValue);
        }
    }
}
