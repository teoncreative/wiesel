namespace WieselEngine
{
    public class LightPointComponent : Component
    {

        public LightPointComponent(Entity entity)
        {
            this.entity = entity;
        }

        public Vector3f Color
        {
            get
            {
                return new Vector3f(
                    Internals.LightPoint_GetColorR(entity.ScenePtr, entity.Id),
                    Internals.LightPoint_GetColorG(entity.ScenePtr, entity.Id),
                    Internals.LightPoint_GetColorB(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.LightPoint_SetColorR(entity.ScenePtr, entity.Id, value.X);
                Internals.LightPoint_SetColorG(entity.ScenePtr, entity.Id, value.Y);
                Internals.LightPoint_SetColorB(entity.ScenePtr, entity.Id, value.Z);
            }
        }

        public float Ambient
        {
            get { return Internals.LightPoint_GetAmbient(entity.ScenePtr, entity.Id); }
            set { Internals.LightPoint_SetAmbient(entity.ScenePtr, entity.Id, value); }
        }

        public float Diffuse
        {
            get { return Internals.LightPoint_GetDiffuse(entity.ScenePtr, entity.Id); }
            set { Internals.LightPoint_SetDiffuse(entity.ScenePtr, entity.Id, value); }
        }

        public float Specular
        {
            get { return Internals.LightPoint_GetSpecular(entity.ScenePtr, entity.Id); }
            set { Internals.LightPoint_SetSpecular(entity.ScenePtr, entity.Id, value); }
        }

        public float Density
        {
            get { return Internals.LightPoint_GetDensity(entity.ScenePtr, entity.Id); }
            set { Internals.LightPoint_SetDensity(entity.ScenePtr, entity.Id, value); }
        }

        public float Constant
        {
            get { return Internals.LightPoint_GetConstant(entity.ScenePtr, entity.Id); }
            set { Internals.LightPoint_SetConstant(entity.ScenePtr, entity.Id, value); }
        }

        public float Linear
        {
            get { return Internals.LightPoint_GetLinear(entity.ScenePtr, entity.Id); }
            set { Internals.LightPoint_SetLinear(entity.ScenePtr, entity.Id, value); }
        }

        public float Exp
        {
            get { return Internals.LightPoint_GetExp(entity.ScenePtr, entity.Id); }
            set { Internals.LightPoint_SetExp(entity.ScenePtr, entity.Id, value); }
        }
    }
}