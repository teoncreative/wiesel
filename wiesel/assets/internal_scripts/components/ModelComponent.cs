using System;

namespace WieselEngine
{
    public class ModelComponent
    {
        private ulong scenePtr;
        private ulong entityId;

        public bool EnableRendering
        {
            get { return Internals.ModelComponent_GetEnableRendering(scenePtr, entityId); }
            set { Internals.ModelComponent_SetEnableRendering(scenePtr, entityId, value); }
        }

        public ModelComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }
    }
}
