using System.Runtime.CompilerServices;

namespace WieselEngine
{
    public static class Time
    {
        public static float DeltaTime
        {
            get { return Internals.Time_GetDeltaTime(); }
        }

        public static float TimeScale
        {
            get { return Internals.Time_GetTimeScale(); }
            set { Internals.Time_SetTimeScale(value); }
        }

        public static float ElapsedTime
        {
            get { return Internals.Time_GetElapsedTime(); }
        }
    }
}