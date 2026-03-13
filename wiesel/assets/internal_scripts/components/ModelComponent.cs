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

        public Vector4f ColorTint
        {
            get { return new Vector4f(
                Internals.ModelComponent_GetColorTintR(scenePtr, entityId),
                Internals.ModelComponent_GetColorTintG(scenePtr, entityId),
                Internals.ModelComponent_GetColorTintB(scenePtr, entityId),
                Internals.ModelComponent_GetColorTintA(scenePtr, entityId)); }
            set {
                Internals.ModelComponent_SetColorTintR(scenePtr, entityId, value.X);
                Internals.ModelComponent_SetColorTintG(scenePtr, entityId, value.Y);
                Internals.ModelComponent_SetColorTintB(scenePtr, entityId, value.Z);
                Internals.ModelComponent_SetColorTintA(scenePtr, entityId, value.W);
            }
        }

        public float Roughness
        {
            get { return Internals.ModelComponent_GetRoughness(scenePtr, entityId); }
            set { Internals.ModelComponent_SetRoughness(scenePtr, entityId, value); }
        }

        public float Metallic
        {
            get { return Internals.ModelComponent_GetMetallic(scenePtr, entityId); }
            set { Internals.ModelComponent_SetMetallic(scenePtr, entityId, value); }
        }

        public float Specular
        {
            get { return Internals.ModelComponent_GetSpecular(scenePtr, entityId); }
            set { Internals.ModelComponent_SetSpecular(scenePtr, entityId, value); }
        }

        public ModelComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }
    }
}
