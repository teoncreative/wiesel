namespace WieselEngine
{
    public class LightDirectComponent : Component
    {

        public LightDirectComponent(Entity entity) : base(entity) { }

        public Vector3f Color
        {
            get
            {
                return new Vector3f(
                    Internals.LightDirect_GetColorR(entity.ScenePtr, entity.Id),
                    Internals.LightDirect_GetColorG(entity.ScenePtr, entity.Id),
                    Internals.LightDirect_GetColorB(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.LightDirect_SetColorR(entity.ScenePtr, entity.Id, value.X);
                Internals.LightDirect_SetColorG(entity.ScenePtr, entity.Id, value.Y);
                Internals.LightDirect_SetColorB(entity.ScenePtr, entity.Id, value.Z);
            }
        }

        public float Ambient
        {
            get { return Internals.LightDirect_GetAmbient(entity.ScenePtr, entity.Id); }
            set { Internals.LightDirect_SetAmbient(entity.ScenePtr, entity.Id, value); }
        }

        public float Diffuse
        {
            get { return Internals.LightDirect_GetDiffuse(entity.ScenePtr, entity.Id); }
            set { Internals.LightDirect_SetDiffuse(entity.ScenePtr, entity.Id, value); }
        }

        public float Specular
        {
            get { return Internals.LightDirect_GetSpecular(entity.ScenePtr, entity.Id); }
            set { Internals.LightDirect_SetSpecular(entity.ScenePtr, entity.Id, value); }
        }

        public float Density
        {
            get { return Internals.LightDirect_GetDensity(entity.ScenePtr, entity.Id); }
            set { Internals.LightDirect_SetDensity(entity.ScenePtr, entity.Id, value); }
        }
    }
}