using System;

namespace WieselEngine
{
    public class AnimatorComponent : Component
    {

        public AnimatorComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        public bool IsPlaying
        {
            get { return Internals.Animator_GetIsPlaying(scenePtr, entityId); }
            set { Internals.Animator_SetIsPlaying(scenePtr, entityId, value); }
        }

        public string CurrentState
        {
            get { return Internals.Animator_GetCurrentState(scenePtr, entityId); }
        }

        public void SetBool(string name, bool value)
        {
            Internals.Animator_SetBool(scenePtr, entityId, name, value);
        }

        public bool GetBool(string name)
        {
            return Internals.Animator_GetBool(scenePtr, entityId, name);
        }

        public void SetInt(string name, int value)
        {
            Internals.Animator_SetInt(scenePtr, entityId, name, value);
        }

        public int GetInt(string name)
        {
            return Internals.Animator_GetInt(scenePtr, entityId, name);
        }

        public void SetFloat(string name, float value)
        {
            Internals.Animator_SetFloat(scenePtr, entityId, name, value);
        }

        public float GetFloat(string name)
        {
            return Internals.Animator_GetFloat(scenePtr, entityId, name);
        }

        public void SetTrigger(string name)
        {
            Internals.Animator_SetTrigger(scenePtr, entityId, name);
        }

        public void Play(string stateName, float blendTime = 0.25f)
        {
            Internals.Animator_Play(scenePtr, entityId, stateName, blendTime);
        }
    }
}
