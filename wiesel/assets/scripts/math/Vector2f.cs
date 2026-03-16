using System;

namespace WieselEngine
{
    public class Vector2f
    {
        protected Func<float> getX;
        protected Func<float> getY;
        protected Action<float> setX;
        protected Action<float> setY;

        private float x = 0.0f;
        public float X
        {
            get { return getX != null ? getX() : x; }
            set { if (setX != null) setX(value); else x = value; }
        }

        private float y = 0.0f;
        public float Y
        {
            get { return getY != null ? getY() : y; }
            set { if (setY != null) setY(value); else y = value; }
        }

        public Vector2f(float x, float y)
        {
            this.x = x;
            this.y = y;
        }

        public Vector2f() { }

        public static Vector2f operator +(Vector2f a, Vector2f b)
        {
            return new Vector2f(a.X + b.X, a.Y + b.Y);
        }

        public static Vector2f operator -(Vector2f a, Vector2f b)
        {
            return new Vector2f(a.X - b.X, a.Y - b.Y);
        }

        public static Vector2f operator *(Vector2f a, float s)
        {
            return new Vector2f(a.X * s, a.Y * s);
        }

        public static Vector2f operator *(float s, Vector2f a)
        {
            return new Vector2f(s * a.X, s * a.Y);
        }

        public static Vector2f operator /(Vector2f a, float s)
        {
            return new Vector2f(a.X / s, a.Y / s);
        }

        public static Vector2f operator -(Vector2f a)
        {
            return new Vector2f(-a.X, -a.Y);
        }

        public float Length()
        {
            return (float)Math.Sqrt(X * X + Y * Y);
        }

        public Vector2f Normalized()
        {
            float len = Length();
            if (len == 0f) return new Vector2f(0, 0);
            return this / len;
        }

        public static float Dot(Vector2f a, Vector2f b)
        {
            return a.X * b.X + a.Y * b.Y;
        }

        public static float Distance(Vector2f a, Vector2f b)
        {
            return (a - b).Length();
        }

        public override string ToString()
        {
            return "Vector2f(" + X + ", " + Y + ")";
        }
    }

    public class HandledVector2f : Vector2f
    {
        public HandledVector2f(Func<float> getX, Action<float> setX,
                               Func<float> getY, Action<float> setY)
        {
            this.getX = getX;
            this.setX = setX;
            this.getY = getY;
            this.setY = setY;
        }
    }
}
