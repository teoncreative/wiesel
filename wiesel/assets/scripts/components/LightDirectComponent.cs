namespace WieselEngine
{
    public class LightDirectComponent : Component
    {

        public LightDirectComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        public Vector3f Color
        {
            get
            {
                return new Vector3f(
                    Internals.LightDirect_GetColorR(scenePtr, entityId),
                    Internals.LightDirect_GetColorG(scenePtr, entityId),
                    Internals.LightDirect_GetColorB(scenePtr, entityId));
            }
            set
            {
                Internals.LightDirect_SetColorR(scenePtr, entityId, value.X);
                Internals.LightDirect_SetColorG(scenePtr, entityId, value.Y);
                Internals.LightDirect_SetColorB(scenePtr, entityId, value.Z);
            }
        }

        public float Ambient
        {
            get { return Internals.LightDirect_GetAmbient(scenePtr, entityId); }
            set { Internals.LightDirect_SetAmbient(scenePtr, entityId, value); }
        }

        public float Diffuse
        {
            get { return Internals.LightDirect_GetDiffuse(scenePtr, entityId); }
            set { Internals.LightDirect_SetDiffuse(scenePtr, entityId, value); }
        }

        public float Specular
        {
            get { return Internals.LightDirect_GetSpecular(scenePtr, entityId); }
            set { Internals.LightDirect_SetSpecular(scenePtr, entityId, value); }
        }

        public float Density
        {
            get { return Internals.LightDirect_GetDensity(scenePtr, entityId); }
            set { Internals.LightDirect_SetDensity(scenePtr, entityId, value); }
        }
    }
}