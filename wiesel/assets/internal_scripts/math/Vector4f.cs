using System;

namespace WieselEngine
{
    public class Vector4f
    {
        protected Func<float> getX;
        protected Func<float> getY;
        protected Func<float> getZ;
        protected Func<float> getW;
        protected Action<float> setX;
        protected Action<float> setY;
        protected Action<float> setZ;
        protected Action<float> setW;

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

        private float z = 0.0f;
        public float Z
        {
            get { return getZ != null ? getZ() : z; }
            set { if (setZ != null) setZ(value); else z = value; }
        }

        private float w = 0.0f;
        public float W
        {
            get { return getW != null ? getW() : w; }
            set { if (setW != null) setW(value); else w = value; }
        }

        public Vector4f(float x, float y, float z, float w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }

        public Vector4f() { }

        public override string ToString()
        {
            return "Vector4f(" + X + ", " + Y + ", " + Z + ", " + W + ")";
        }
    }

    public class HandledVector4f : Vector4f
    {
        public HandledVector4f(Func<float> getX, Action<float> setX,
                               Func<float> getY, Action<float> setY,
                               Func<float> getZ, Action<float> setZ,
                               Func<float> getW, Action<float> setW)
        {
            this.getX = getX;
            this.setX = setX;
            this.getY = getY;
            this.setY = setY;
            this.getZ = getZ;
            this.setZ = setZ;
            this.getW = getW;
            this.setW = setW;
        }
    }
}
