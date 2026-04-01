using System;

namespace WieselEngine
{
    public class ModelComponent : Component
    {

        public bool EnableRendering
        {
            get { return Internals.ModelComponent_GetEnableRendering(entity.ScenePtr, entity.Id); }
            set { Internals.ModelComponent_SetEnableRendering(entity.ScenePtr, entity.Id, value); }
        }

        public Vector4f ColorTint
        {
            get { return new Vector4f(
                Internals.ModelComponent_GetColorTintR(entity.ScenePtr, entity.Id),
                Internals.ModelComponent_GetColorTintG(entity.ScenePtr, entity.Id),
                Internals.ModelComponent_GetColorTintB(entity.ScenePtr, entity.Id),
                Internals.ModelComponent_GetColorTintA(entity.ScenePtr, entity.Id)); }
            set {
                Internals.ModelComponent_SetColorTintR(entity.ScenePtr, entity.Id, value.X);
                Internals.ModelComponent_SetColorTintG(entity.ScenePtr, entity.Id, value.Y);
                Internals.ModelComponent_SetColorTintB(entity.ScenePtr, entity.Id, value.Z);
                Internals.ModelComponent_SetColorTintA(entity.ScenePtr, entity.Id, value.W);
            }
        }

        public float Roughness
        {
            get { return Internals.ModelComponent_GetRoughness(entity.ScenePtr, entity.Id); }
            set { Internals.ModelComponent_SetRoughness(entity.ScenePtr, entity.Id, value); }
        }

        public float Metallic
        {
            get { return Internals.ModelComponent_GetMetallic(entity.ScenePtr, entity.Id); }
            set { Internals.ModelComponent_SetMetallic(entity.ScenePtr, entity.Id, value); }
        }

        public float Specular
        {
            get { return Internals.ModelComponent_GetSpecular(entity.ScenePtr, entity.Id); }
            set { Internals.ModelComponent_SetSpecular(entity.ScenePtr, entity.Id, value); }
        }

        public ModelComponent(Entity entity)
        {
            this.entity = entity;
        }
    }
}
