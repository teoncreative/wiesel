namespace WieselEngine
{
    public class SpriteAnimatorComponent
    {
        private ulong scenePtr;
        private ulong entityId;

        public SpriteAnimatorComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        public void Play(string stateName, bool restart = true)
        {
            Internals.SpriteAnimator_Play(scenePtr, entityId, stateName, restart);
        }

        public void Stop()
        {
            Internals.SpriteAnimator_Stop(scenePtr, entityId);
        }

        public bool IsPlaying
        {
            get { return Internals.SpriteAnimator_GetIsPlaying(scenePtr, entityId); }
        }

        public string CurrentState
        {
            get { return Internals.SpriteAnimator_GetCurrentState(scenePtr, entityId); }
        }

        public int CurrentFrame
        {
            get { return Internals.SpriteAnimator_GetCurrentFrame(scenePtr, entityId); }
        }

        public void SetBool(string name, bool value)
        {
            Internals.SpriteAnimator_SetBool(scenePtr, entityId, name, value);
        }

        public void SetInt(string name, int value)
        {
            Internals.SpriteAnimator_SetInt(scenePtr, entityId, name, value);
        }

        public void SetFloat(string name, float value)
        {
            Internals.SpriteAnimator_SetFloat(scenePtr, entityId, name, value);
        }

        public void SetTrigger(string name)
        {
            Internals.SpriteAnimator_SetTrigger(scenePtr, entityId, name);
        }

        public bool GetBool(string name)
        {
            return Internals.SpriteAnimator_GetBool(scenePtr, entityId, name);
        }

        public int GetInt(string name)
        {
            return Internals.SpriteAnimator_GetInt(scenePtr, entityId, name);
        }

        public float GetFloat(string name)
        {
            return Internals.SpriteAnimator_GetFloat(scenePtr, entityId, name);
        }
    }
}
