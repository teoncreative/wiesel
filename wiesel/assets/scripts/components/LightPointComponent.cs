namespace WieselEngine
{
    public class LightPointComponent : Component
    {

        public LightPointComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        public Vector3f Color
        {
            get
            {
                return new Vector3f(
                    Internals.LightPoint_GetColorR(scenePtr, entityId),
                    Internals.LightPoint_GetColorG(scenePtr, entityId),
                    Internals.LightPoint_GetColorB(scenePtr, entityId));
            }
            set
            {
                Internals.LightPoint_SetColorR(scenePtr, entityId, value.X);
                Internals.LightPoint_SetColorG(scenePtr, entityId, value.Y);
                Internals.LightPoint_SetColorB(scenePtr, entityId, value.Z);
            }
        }

        public float Ambient
        {
            get { return Internals.LightPoint_GetAmbient(scenePtr, entityId); }
            set { Internals.LightPoint_SetAmbient(scenePtr, entityId, value); }
        }

        public float Diffuse
        {
            get { return Internals.LightPoint_GetDiffuse(scenePtr, entityId); }
            set { Internals.LightPoint_SetDiffuse(scenePtr, entityId, value); }
        }

        public float Specular
        {
            get { return Internals.LightPoint_GetSpecular(scenePtr, entityId); }
            set { Internals.LightPoint_SetSpecular(scenePtr, entityId, value); }
        }

        public float Density
        {
            get { return Internals.LightPoint_GetDensity(scenePtr, entityId); }
            set { Internals.LightPoint_SetDensity(scenePtr, entityId, value); }
        }

        public float Constant
        {
            get { return Internals.LightPoint_GetConstant(scenePtr, entityId); }
            set { Internals.LightPoint_SetConstant(scenePtr, entityId, value); }
        }

        public float Linear
        {
            get { return Internals.LightPoint_GetLinear(scenePtr, entityId); }
            set { Internals.LightPoint_SetLinear(scenePtr, entityId, value); }
        }

        public float Exp
        {
            get { return Internals.LightPoint_GetExp(scenePtr, entityId); }
            set { Internals.LightPoint_SetExp(scenePtr, entityId, value); }
        }
    }
}