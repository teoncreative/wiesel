namespace WieselEngine
{
    public class AudioSourceComponent : Component
    {

        public AudioSourceComponent(ulong scenePtr, ulong entityId)
        {
            this.scenePtr = scenePtr;
            this.entityId = entityId;
        }

        // Play the default clip attached to this source
        public void Play()
        {
            Internals.AudioSource_Play(scenePtr, entityId);
        }

        // Play a specific clip through this source (uses source's spatial settings)
        public void Play(AudioClip clip)
        {
            if (clip == null || !clip.IsValid()) return;
            Internals.AudioSource_PlayClip(scenePtr, entityId, clip.handle);
        }

        // Stop the currently playing sound
        public void Stop()
        {
            Internals.AudioSource_Stop(scenePtr, entityId);
        }

        public bool IsPlaying
        {
            get { return Internals.AudioSource_GetIsPlaying(scenePtr, entityId); }
        }

        public float Volume
        {
            get { return Internals.AudioSource_GetVolume(scenePtr, entityId); }
            set { Internals.AudioSource_SetVolume(scenePtr, entityId, value); }
        }

        public float Pitch
        {
            get { return Internals.AudioSource_GetPitch(scenePtr, entityId); }
            set { Internals.AudioSource_SetPitch(scenePtr, entityId, value); }
        }

        public bool Loop
        {
            get { return Internals.AudioSource_GetLoop(scenePtr, entityId); }
            set { Internals.AudioSource_SetLoop(scenePtr, entityId, value); }
        }

        public bool Mute
        {
            get { return Internals.AudioSource_GetMute(scenePtr, entityId); }
            set { Internals.AudioSource_SetMute(scenePtr, entityId, value); }
        }

        public float SpatialBlend
        {
            get { return Internals.AudioSource_GetSpatialBlend(scenePtr, entityId); }
            set { Internals.AudioSource_SetSpatialBlend(scenePtr, entityId, value); }
        }
    }
}