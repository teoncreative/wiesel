namespace WieselEngine
{
    public enum ProjectionMode : int
    {
        Perspective = 0,
        Orthographic = 1
    }

    public class CameraComponent : Component
    {

        public CameraComponent(Entity entity) : base(entity) { }

        public ProjectionMode Projection
        {
            get { return (ProjectionMode)Internals.Camera_GetProjectionMode(entity.ScenePtr, entity.Id); }
            set { Internals.Camera_SetProjectionMode(entity.ScenePtr, entity.Id, (int)value); }
        }

        public float FieldOfView
        {
            get { return Internals.Camera_GetFOV(entity.ScenePtr, entity.Id); }
            set { Internals.Camera_SetFOV(entity.ScenePtr, entity.Id, value); }
        }

        public float OrthoSize
        {
            get { return Internals.Camera_GetOrthoSize(entity.ScenePtr, entity.Id); }
            set { Internals.Camera_SetOrthoSize(entity.ScenePtr, entity.Id, value); }
        }

        public float NearPlane
        {
            get { return Internals.Camera_GetNearPlane(entity.ScenePtr, entity.Id); }
            set { Internals.Camera_SetNearPlane(entity.ScenePtr, entity.Id, value); }
        }

        public float FarPlane
        {
            get { return Internals.Camera_GetFarPlane(entity.ScenePtr, entity.Id); }
            set { Internals.Camera_SetFarPlane(entity.ScenePtr, entity.Id, value); }
        }

        public bool Enabled
        {
            get { return Internals.Camera_GetEnabled(entity.ScenePtr, entity.Id); }
            set { Internals.Camera_SetEnabled(entity.ScenePtr, entity.Id, value); }
        }

        public Vector4f BackgroundColor
        {
            get
            {
                return new Vector4f(
                    Internals.Camera_GetBgColorR(entity.ScenePtr, entity.Id),
                    Internals.Camera_GetBgColorG(entity.ScenePtr, entity.Id),
                    Internals.Camera_GetBgColorB(entity.ScenePtr, entity.Id),
                    Internals.Camera_GetBgColorA(entity.ScenePtr, entity.Id));
            }
            set
            {
                Internals.Camera_SetBgColorR(entity.ScenePtr, entity.Id, value.X);
                Internals.Camera_SetBgColorG(entity.ScenePtr, entity.Id, value.Y);
                Internals.Camera_SetBgColorB(entity.ScenePtr, entity.Id, value.Z);
                Internals.Camera_SetBgColorA(entity.ScenePtr, entity.Id, value.W);
            }
        }
    }
}