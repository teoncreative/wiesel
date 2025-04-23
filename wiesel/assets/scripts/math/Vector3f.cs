using System;

namespace WieselEngine
{
    public class Vector3f
    {
        public static readonly Vector3f Up = new Vector3f(0, 1, 0);
        public static readonly Vector3f Down = new Vector3f(0, -1, 0);
        public static readonly Vector3f Left = new Vector3f(-1, 0, 0);
        public static readonly Vector3f Right = new Vector3f(1, 0, 0);
        public static readonly Vector3f Forward = new Vector3f(0, 0, 1);
        public static readonly Vector3f Back = new Vector3f(0, 0, -1);

        protected Func<float> getX;
        protected Func<float> getY;
        protected Func<float> getZ;
        protected Action<float> setX;
        protected Action<float> setY;
        protected Action<float> setZ;

        private float x = 0.0f;
        public float X
        {
            get
            {
                if (getX != null)
                {
                    return getX();
                }
                return x;
            }
            set
            {
                if (setX != null)
                {
                    setX(value);
                    return;
                }
                x = value;
            }
        }

        private float y = 0.0f;
        public float Y
        {
            get
            {
                if (getY != null)
                {
                    return getY();
                }

                return y;
            }
            set
            {
                if (setY != null)
                {
                    setY(value);
                    return;
                }
                y = value;
            }
        }

        private float z = 0.0f;
        public float Z
        {
            get
            {
                if (getZ != null)
                {
                    return getZ();
                }
                return z;
            }
            set
            {
                if (setZ != null)
                {
                    setZ(value);
                    return;
                }
                z = value;
            }
        }

        public Vector3f(float x, float y, float z) {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public Vector3f(float x, float y) {
            this.x = x;
            this.y = y;
            this.z = 0.0f;
        }

        public Vector3f(float x) {
            this.x = x;
            this.y = 0.0f;
            this.z = 0.0f;
        }

        public Vector3f() {
            this.x = 0.0f;
            this.y = 0.0f;
            this.z = 0.0f;
        }

        public static Vector3f operator*(Vector3f lhs, Vector3f rhs) {
            return new Vector3f(lhs.X * rhs.X, lhs.Y * rhs.Y, lhs.Z * rhs.Z);
        }

        public static Vector3f operator*(Vector3f lhs, float rhs) {
            return new Vector3f(lhs.X * rhs, lhs.Y * rhs, lhs.Z * rhs);
        }

        public static Vector3f operator+(Vector3f lhs, Vector3f rhs) {
            return new Vector3f(lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z);
        }

        public static Vector3f operator+(Vector3f lhs, float rhs) {
            return new Vector3f(lhs.X + rhs, lhs.Y + rhs, lhs.Z + rhs);
        }

        public static Vector3f operator/(Vector3f lhs, Vector3f rhs) {
            // todo divide by zero check??
            return new Vector3f(lhs.X / rhs.X, lhs.Y / rhs.Y, lhs.Z / rhs.Z);
        }

        public static Vector3f operator/(Vector3f lhs, float rhs) {
            // todo divide by zero check??
            return new Vector3f(lhs.X / rhs, lhs.Y / rhs, lhs.Z / rhs);
        }

        public static Vector3f operator-(Vector3f lhs, Vector3f rhs) {
            return new Vector3f(lhs.X - rhs.X, lhs.Y - rhs.Y, lhs.Z - rhs.Z);
        }

        public static Vector3f operator-(Vector3f lhs, float rhs) {
            return new Vector3f(lhs.X - rhs, lhs.Y - rhs, lhs.Z - rhs);
        }

        public static Vector3f operator-(Vector3f lhs) {
            return new Vector3f(-lhs.X, -lhs.Y, -lhs.Z);
        }

        public static Vector3f operator +(Vector3f v)
        {
            return new Vector3f(+v.X, +v.Y, +v.Z);
        }

        public float Length()
        {
            return (float)Math.Sqrt(X * X + Y * Y + Z * Z);
        }

        public Vector3f Normalized()
        {
            float length = Length();
            if (length == 0f)
                return new Vector3f(0, 0, 0);
            return this / length;
        }

        public override string ToString()
        {
          return base.ToString() + ": x:" + X + ", y:" + Y + ", z:" + Z;
        }
    }

    public class HandledVector3f : Vector3f
    {
        public HandledVector3f(Func<float> getX, Action<float> setX, Func<float> getY, Action<float> setY, Func<float> getZ, Action<float> setZ)
        {
            this.getX = getX;
            this.setX = setX;
            this.getY = getY;
            this.setY = setY;
            this.getZ = getZ;
            this.setZ = setZ;
        }
    }

}