namespace WieselEngine
{
    public enum ProjectionMode : int
    {
        Perspective = 0,
        Orthographic = 1
    }

    public class CameraComponent
    {
        private ulong scenePtr;
        private ulong entityId;

        public CameraComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        public ProjectionMode Projection
        {
            get { return (ProjectionMode)Internals.Camera_GetProjectionMode(scenePtr, entityId); }
            set { Internals.Camera_SetProjectionMode(scenePtr, entityId, (int)value); }
        }

        public float FieldOfView
        {
            get { return Internals.Camera_GetFOV(scenePtr, entityId); }
            set { Internals.Camera_SetFOV(scenePtr, entityId, value); }
        }

        public float OrthoSize
        {
            get { return Internals.Camera_GetOrthoSize(scenePtr, entityId); }
            set { Internals.Camera_SetOrthoSize(scenePtr, entityId, value); }
        }

        public float NearPlane
        {
            get { return Internals.Camera_GetNearPlane(scenePtr, entityId); }
            set { Internals.Camera_SetNearPlane(scenePtr, entityId, value); }
        }

        public float FarPlane
        {
            get { return Internals.Camera_GetFarPlane(scenePtr, entityId); }
            set { Internals.Camera_SetFarPlane(scenePtr, entityId, value); }
        }

        public bool Enabled
        {
            get { return Internals.Camera_GetEnabled(scenePtr, entityId); }
            set { Internals.Camera_SetEnabled(scenePtr, entityId, value); }
        }

        public Vector4f BackgroundColor
        {
            get
            {
                return new Vector4f(
                    Internals.Camera_GetBgColorR(scenePtr, entityId),
                    Internals.Camera_GetBgColorG(scenePtr, entityId),
                    Internals.Camera_GetBgColorB(scenePtr, entityId),
                    Internals.Camera_GetBgColorA(scenePtr, entityId));
            }
            set
            {
                Internals.Camera_SetBgColorR(scenePtr, entityId, value.X);
                Internals.Camera_SetBgColorG(scenePtr, entityId, value.Y);
                Internals.Camera_SetBgColorB(scenePtr, entityId, value.Z);
                Internals.Camera_SetBgColorA(scenePtr, entityId, value.W);
            }
        }
    }
}