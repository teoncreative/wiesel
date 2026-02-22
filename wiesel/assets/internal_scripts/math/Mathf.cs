using System;

namespace WieselEngine
{
    public class Mathf
    {
        public static readonly float PI = (float)Math.PI;
        public static readonly float Deg2Rad = PI / 180.0f;
        public static readonly float Rad2Deg = 180.0f / PI;

        public static float Abs(float value) { return Math.Abs(value); }
        public static float Sqrt(float value) { return (float)Math.Sqrt(value); }
        public static float Sign(float value) { return value > 0f ? 1f : (value < 0f ? -1f : 0f); }

        public static float Min(float a, float b) { return a < b ? a : b; }
        public static float Max(float a, float b) { return a > b ? a : b; }

        public static float Clamp(float value, float min, float max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }

        public static float Clamp01(float value)
        {
            return Clamp(value, 0f, 1f);
        }

        public static float Lerp(float a, float b, float t)
        {
            return a + (b - a) * Clamp01(t);
        }

        public static float LerpUnclamped(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        public static float MoveTowards(float current, float target, float maxDelta)
        {
            if (Abs(target - current) <= maxDelta)
                return target;
            return current + Sign(target - current) * maxDelta;
        }

        public static float Floor(float value) { return (float)Math.Floor(value); }
        public static float Ceil(float value) { return (float)Math.Ceiling(value); }
        public static float Round(float value) { return (float)Math.Round(value); }

        public static float Sin(float value) { return (float)Math.Sin(value); }
        public static float Cos(float value) { return (float)Math.Cos(value); }
        public static float Atan2(float y, float x) { return (float)Math.Atan2(y, x); }
    }

}