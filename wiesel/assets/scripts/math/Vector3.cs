using System;

namespace WieselEngine
{
    public class Vector3f
    {
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
                    return getX();
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