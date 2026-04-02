namespace WieselEngine
{
    public enum AntiAliasingMode { None = 0, FXAA = 1, TAA = 2 }

    /// <summary>
    /// Static API for reading and writing engine settings at runtime.
    /// </summary>
    public static class Settings
    {
        // -- Graphics --

        public static bool VSync
        {
            get { return Internals.Settings_GetVSync(); }
            set { Internals.Settings_SetVSync(value); }
        }

        public static bool SSAO
        {
            get { return Internals.Settings_GetSSAO(); }
            set { Internals.Settings_SetSSAO(value); }
        }

        public static bool Bloom
        {
            get { return Internals.Settings_GetBloom(); }
            set { Internals.Settings_SetBloom(value); }
        }

        public static float BloomIntensity
        {
            get { return Internals.Settings_GetBloomIntensity(); }
            set { Internals.Settings_SetBloomIntensity(value); }
        }

        public static bool MotionBlur
        {
            get { return Internals.Settings_GetMotionBlur(); }
            set { Internals.Settings_SetMotionBlur(value); }
        }

        public static float MotionBlurStrength
        {
            get { return Internals.Settings_GetMotionBlurStrength(); }
            set { Internals.Settings_SetMotionBlurStrength(value); }
        }

        public static bool Shadows
        {
            get { return Internals.Settings_GetShadows(); }
            set { Internals.Settings_SetShadows(value); }
        }

        public static bool RTShadows
        {
            get { return Internals.Settings_GetRTShadows(); }
            set { Internals.Settings_SetRTShadows(value); }
        }

        public static bool IsRTSupported
        {
            get { return Internals.Settings_IsRTSupported(); }
        }

        public static AntiAliasingMode AAMode
        {
            get { return (AntiAliasingMode)Internals.Settings_GetAAMode(); }
            set { Internals.Settings_SetAAMode((int)value); }
        }

        // -- Audio --

        public static float MasterVolume
        {
            get { return Internals.Settings_GetMasterVolume(); }
            set { Internals.Settings_SetMasterVolume(value); }
        }

        public static float MusicVolume
        {
            get { return Internals.Settings_GetMusicVolume(); }
            set { Internals.Settings_SetMusicVolume(value); }
        }

        public static float SFXVolume
        {
            get { return Internals.Settings_GetSFXVolume(); }
            set { Internals.Settings_SetSFXVolume(value); }
        }
    }
}
