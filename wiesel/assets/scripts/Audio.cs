namespace WieselEngine
{
    public enum AudioBus : int
    {
        Master = 0,
        SFX = 1,
        Music = 2
    }

    public class Audio
    {
        // Fire-and-forget 2D by VFS path
        public static void Play(string path, AudioBus bus = AudioBus.SFX, float volume = 1.0f)
        {
            Internals.Audio_PlayPath(path, (int)bus, volume, 1.0f, false);
        }

        // Fire-and-forget 2D by AudioClip
        public static void Play(AudioClip clip, AudioBus bus = AudioBus.SFX, float volume = 1.0f)
        {
            if (clip == null || !clip.IsValid()) return;
            Internals.Audio_PlayClip(clip.handle, (int)bus, volume, 1.0f, false);
        }

        // Fire-and-forget 3D by VFS path
        public static void PlayAt(string path, Vector3f position,
                                   AudioBus bus = AudioBus.SFX, float volume = 1.0f,
                                   float minDistance = 1.0f, float maxDistance = 100.0f)
        {
            Internals.Audio_PlayAtPath(path, position.X, position.Y, position.Z,
                                        (int)bus, volume, minDistance, maxDistance);
        }

        // Fire-and-forget 3D by AudioClip
        public static void PlayAt(AudioClip clip, Vector3f position,
                                    AudioBus bus = AudioBus.SFX, float volume = 1.0f,
                                    float minDistance = 1.0f, float maxDistance = 100.0f)
        {
            if (clip == null || !clip.IsValid()) return;
            Internals.Audio_PlayAtClip(clip.handle, position.X, position.Y, position.Z,
                                        (int)bus, volume, minDistance, maxDistance);
        }

        // Music
        public static void PlayMusic(string path, float volume = 1.0f)
        {
            Internals.Audio_PlayMusic(path, volume);
        }

        public static void PlayMusic(AudioClip clip, float volume = 1.0f)
        {
            if (clip == null || !clip.IsValid()) return;
            Internals.Audio_PlayMusicClip(clip.handle, volume);
        }

        public static void StopMusic()
        {
            Internals.Audio_StopMusic();
        }

        // Volume
        public static float MasterVolume
        {
            get { return Internals.Audio_GetMasterVolume(); }
            set { Internals.Audio_SetMasterVolume(value); }
        }

        public static float SFXVolume
        {
            get { return Internals.Audio_GetSFXVolume(); }
            set { Internals.Audio_SetSFXVolume(value); }
        }

        public static float MusicVolume
        {
            get { return Internals.Audio_GetMusicVolume(); }
            set { Internals.Audio_SetMusicVolume(value); }
        }
    }
}