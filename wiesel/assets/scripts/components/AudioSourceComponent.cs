namespace WieselEngine
{
    public class AudioSourceComponent : Component
    {

        public AudioSourceComponent(Entity entity) : base(entity) { }

        // Play the default clip attached to this source
        public void Play()
        {
            Internals.AudioSource_Play(entity.ScenePtr, entity.Id);
        }

        // Play a specific clip through this source (uses source's spatial settings)
        public void Play(AudioClip clip)
        {
            if (clip == null || !clip.IsValid()) return;
            Internals.AudioSource_PlayClip(entity.ScenePtr, entity.Id, clip.Handle);
        }

        // Stop the currently playing sound
        public void Stop()
        {
            Internals.AudioSource_Stop(entity.ScenePtr, entity.Id);
        }

        public bool IsPlaying
        {
            get { return Internals.AudioSource_GetIsPlaying(entity.ScenePtr, entity.Id); }
        }

        public float Volume
        {
            get { return Internals.AudioSource_GetVolume(entity.ScenePtr, entity.Id); }
            set { Internals.AudioSource_SetVolume(entity.ScenePtr, entity.Id, value); }
        }

        public float Pitch
        {
            get { return Internals.AudioSource_GetPitch(entity.ScenePtr, entity.Id); }
            set { Internals.AudioSource_SetPitch(entity.ScenePtr, entity.Id, value); }
        }

        public bool Loop
        {
            get { return Internals.AudioSource_GetLoop(entity.ScenePtr, entity.Id); }
            set { Internals.AudioSource_SetLoop(entity.ScenePtr, entity.Id, value); }
        }

        public bool Mute
        {
            get { return Internals.AudioSource_GetMute(entity.ScenePtr, entity.Id); }
            set { Internals.AudioSource_SetMute(entity.ScenePtr, entity.Id, value); }
        }

        public float SpatialBlend
        {
            get { return Internals.AudioSource_GetSpatialBlend(entity.ScenePtr, entity.Id); }
            set { Internals.AudioSource_SetSpatialBlend(entity.ScenePtr, entity.Id, value); }
        }
    }
}