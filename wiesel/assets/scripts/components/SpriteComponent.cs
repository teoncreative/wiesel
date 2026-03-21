namespace WieselEngine
{
    public class SpriteComponent
    {
        private ulong scenePtr;
        private ulong entityId;

        public SpriteComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        // Play a clip by name
        public void Play(string clipName, bool restart = true)
        {
            Internals.Sprite_Play(scenePtr, entityId, clipName, restart);
        }

        public void Stop()
        {
            Internals.Sprite_Stop(scenePtr, entityId);
        }

        public bool IsPlaying
        {
            get { return Internals.Sprite_GetIsPlaying(scenePtr, entityId); }
        }

        public string CurrentClip
        {
            get { return Internals.Sprite_GetCurrentClip(scenePtr, entityId); }
        }

        public int CurrentFrame
        {
            get { return Internals.Sprite_GetCurrentFrame(scenePtr, entityId); }
        }

        // State machine parameters
        public void SetBool(string name, bool value)
        {
            Internals.Sprite_SetBool(scenePtr, entityId, name, value);
        }

        public void SetInt(string name, int value)
        {
            Internals.Sprite_SetInt(scenePtr, entityId, name, value);
        }

        public void SetFloat(string name, float value)
        {
            Internals.Sprite_SetFloat(scenePtr, entityId, name, value);
        }

        public void SetTrigger(string name)
        {
            Internals.Sprite_SetTrigger(scenePtr, entityId, name);
        }

        public bool GetBool(string name)
        {
            return Internals.Sprite_GetBool(scenePtr, entityId, name);
        }

        public int GetInt(string name)
        {
            return Internals.Sprite_GetInt(scenePtr, entityId, name);
        }

        public float GetFloat(string name)
        {
            return Internals.Sprite_GetFloat(scenePtr, entityId, name);
        }

        // Visual properties
        public bool FlipX
        {
            get { return Internals.Sprite_GetFlipX(scenePtr, entityId); }
            set { Internals.Sprite_SetFlipX(scenePtr, entityId, value); }
        }

        public bool FlipY
        {
            get { return Internals.Sprite_GetFlipY(scenePtr, entityId); }
            set { Internals.Sprite_SetFlipY(scenePtr, entityId, value); }
        }

        public Vector4f Tint
        {
            get
            {
                return new Vector4f(
                    Internals.Sprite_GetTintR(scenePtr, entityId),
                    Internals.Sprite_GetTintG(scenePtr, entityId),
                    Internals.Sprite_GetTintB(scenePtr, entityId),
                    Internals.Sprite_GetTintA(scenePtr, entityId));
            }
            set
            {
                Internals.Sprite_SetTintR(scenePtr, entityId, value.X);
                Internals.Sprite_SetTintG(scenePtr, entityId, value.Y);
                Internals.Sprite_SetTintB(scenePtr, entityId, value.Z);
                Internals.Sprite_SetTintA(scenePtr, entityId, value.W);
            }
        }

        public int SortLayer
        {
            get { return Internals.Sprite_GetSortLayer(scenePtr, entityId); }
            set { Internals.Sprite_SetSortLayer(scenePtr, entityId, value); }
        }
    }
}
