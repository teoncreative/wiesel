using System;

namespace WieselEngine
{
    public class AnimatorComponent : Component
    {

        public AnimatorComponent(Entity entity)
        {
            this.entity = entity;
        }

        public bool IsPlaying
        {
            get { return Internals.Animator_GetIsPlaying(entity.ScenePtr, entity.Id); }
            set { Internals.Animator_SetIsPlaying(entity.ScenePtr, entity.Id, value); }
        }

        public string CurrentState
        {
            get { return Internals.Animator_GetCurrentState(entity.ScenePtr, entity.Id); }
        }

        public void SetBool(string name, bool value)
        {
            Internals.Animator_SetBool(entity.ScenePtr, entity.Id, name, value);
        }

        public bool GetBool(string name)
        {
            return Internals.Animator_GetBool(entity.ScenePtr, entity.Id, name);
        }

        public void SetInt(string name, int value)
        {
            Internals.Animator_SetInt(entity.ScenePtr, entity.Id, name, value);
        }

        public int GetInt(string name)
        {
            return Internals.Animator_GetInt(entity.ScenePtr, entity.Id, name);
        }

        public void SetFloat(string name, float value)
        {
            Internals.Animator_SetFloat(entity.ScenePtr, entity.Id, name, value);
        }

        public float GetFloat(string name)
        {
            return Internals.Animator_GetFloat(entity.ScenePtr, entity.Id, name);
        }

        public void SetTrigger(string name)
        {
            Internals.Animator_SetTrigger(entity.ScenePtr, entity.Id, name);
        }

        public void Play(string stateName, float blendTime = 0.25f)
        {
            Internals.Animator_Play(entity.ScenePtr, entity.Id, stateName, blendTime);
        }
    }
}
