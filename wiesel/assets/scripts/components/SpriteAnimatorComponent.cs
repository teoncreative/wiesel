namespace WieselEngine
{
    public class SpriteAnimatorComponent : Component
    {

        public SpriteAnimatorComponent(Entity entity)
        {
            this.entity = entity;
        }

        public void Play(string stateName, bool restart = true)
        {
            Internals.SpriteAnimator_Play(entity.ScenePtr, entity.Id, stateName, restart);
        }

        public void Stop()
        {
            Internals.SpriteAnimator_Stop(entity.ScenePtr, entity.Id);
        }

        public bool IsPlaying
        {
            get { return Internals.SpriteAnimator_GetIsPlaying(entity.ScenePtr, entity.Id); }
        }

        public string CurrentState
        {
            get { return Internals.SpriteAnimator_GetCurrentState(entity.ScenePtr, entity.Id); }
        }

        public int CurrentFrame
        {
            get { return Internals.SpriteAnimator_GetCurrentFrame(entity.ScenePtr, entity.Id); }
        }

        public void SetBool(string name, bool value)
        {
            Internals.SpriteAnimator_SetBool(entity.ScenePtr, entity.Id, name, value);
        }

        public void SetInt(string name, int value)
        {
            Internals.SpriteAnimator_SetInt(entity.ScenePtr, entity.Id, name, value);
        }

        public void SetFloat(string name, float value)
        {
            Internals.SpriteAnimator_SetFloat(entity.ScenePtr, entity.Id, name, value);
        }

        public void SetTrigger(string name)
        {
            Internals.SpriteAnimator_SetTrigger(entity.ScenePtr, entity.Id, name);
        }

        public bool GetBool(string name)
        {
            return Internals.SpriteAnimator_GetBool(entity.ScenePtr, entity.Id, name);
        }

        public int GetInt(string name)
        {
            return Internals.SpriteAnimator_GetInt(entity.ScenePtr, entity.Id, name);
        }

        public float GetFloat(string name)
        {
            return Internals.SpriteAnimator_GetFloat(entity.ScenePtr, entity.Id, name);
        }
    }
}
