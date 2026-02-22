using System;
using WieselEngine;

public class CarScript : MonoBehavior
{

    private TransformComponent transform;
    public TransformComponent CameraTransform;

    public float steer = 0.0f;
    public float throttle = 0.0f;

    public float steerStep = 0.5f;
    public float acceleration = 0.08f;
    public float maxSpeed = 0.5f;
    public float minSpeed = -0.2f;
    public float drag = 0.998f;
    public float steerClamp = 30.0f;

    // Building collision boxes: {minX, maxX, minZ, maxZ}
    // Extracted from city model geometry with 0.05 padding
    private static float[][] collisionBoxes = new float[][] {
        new float[] {-0.401f, 0.721f, -3.318f, -2.339f},
        new float[] {-11.685f, -11.248f, -0.840f, 1.530f},
        new float[] {-9.982f, -9.068f, 0.568f, 1.422f},
        new float[] {3.185f, 4.425f, -10.268f, -9.778f},
        new float[] {2.043f, 3.283f, -10.268f, -9.778f},
        new float[] {0.826f, 2.066f, -10.268f, -9.778f},
        new float[] {-5.376f, -4.136f, 1.017f, 1.507f},
        new float[] {9.670f, 10.160f, 6.925f, 8.165f},
        new float[] {-11.481f, -10.560f, 4.875f, 6.175f},
        new float[] {-4.871f, -4.053f, -4.490f, -3.673f},
        new float[] {7.177f, 8.275f, -9.349f, -8.418f},
        new float[] {3.149f, 4.056f, 2.179f, 4.441f},
        new float[] {0.112f, 2.367f, -8.551f, -5.011f},
        new float[] {-5.038f, -4.362f, -1.673f, -1.056f},
        new float[] {-5.038f, -4.362f, -2.829f, -2.212f},
        new float[] {-5.038f, -4.362f, -0.117f, 0.500f},
        new float[] {-0.815f, 0.894f, -10.315f, -9.795f},
        new float[] {-4.180f, -2.444f, -10.138f, -9.784f},
        new float[] {-3.425f, -0.120f, 2.101f, 5.406f},
        new float[] {-0.162f, 2.509f, 2.108f, 4.110f},
        new float[] {-10.822f, -8.450f, -10.180f, -7.808f},
        new float[] {-6.645f, -4.101f, -7.685f, -5.140f},
        new float[] {-5.197f, -4.077f, -8.549f, -7.627f},
        new float[] {-2.476f, -0.767f, -10.315f, -9.795f},
        new float[] {12.974f, 16.529f, 13.068f, 16.623f},
        new float[] {10.843f, 12.845f, 6.834f, 9.506f},
        new float[] {13.771f, 14.939f, 6.847f, 9.200f},
        new float[] {14.249f, 14.936f, 9.168f, 10.460f},
        new float[] {4.413f, 6.788f, -10.227f, -9.184f},
        new float[] {-5.836f, -4.099f, -10.138f, -9.784f},
        new float[] {6.751f, 7.683f, -9.758f, -8.659f},
        new float[] {7.962f, 8.885f, -8.873f, -7.785f},
        new float[] {8.180f, 9.270f, -8.266f, -7.341f},
        new float[] {8.836f, 12.480f, -7.871f, -4.204f},
        new float[] {-8.465f, -5.756f, -10.570f, -9.668f},
        new float[] {-11.228f, -10.311f, -8.097f, -7.016f},
        new float[] {-11.590f, -10.507f, -7.476f, -6.558f},
        new float[] {-11.901f, -10.813f, -6.694f, -5.607f},
        new float[] {-11.786f, -11.228f, -4.101f, -2.932f},
        new float[] {-9.990f, -7.168f, -4.497f, -1.675f},
        new float[] {-12.052f, -11.044f, -5.834f, -4.826f},
        new float[] {-12.107f, -11.177f, -4.977f, -4.048f},
        new float[] {-11.786f, -11.228f, -3.030f, -1.861f},
        new float[] {-11.786f, -11.228f, -1.973f, -0.805f},
        new float[] {-11.685f, -11.248f, 1.417f, 3.786f},
        new float[] {-11.661f, -11.019f, 3.741f, 5.024f},
        new float[] {10.399f, 13.948f, 14.747f, 18.295f},
        new float[] {7.684f, 10.473f, 14.778f, 16.320f},
        new float[] {-3.457f, -2.835f, -4.496f, -3.697f},
        new float[] {5.538f, 7.986f, 13.416f, 15.203f},
        new float[] {3.461f, 5.852f, 11.960f, 13.848f},
        new float[] {2.102f, 3.778f, 11.015f, 12.349f},
        new float[] {0.674f, 2.430f, 10.470f, 11.510f},
        new float[] {-1.981f, 0.892f, 9.619f, 10.977f},
        new float[] {-3.041f, -1.738f, 9.239f, 10.031f},
        new float[] {-11.004f, -9.929f, 5.926f, 7.306f},
        new float[] {-9.117f, -7.200f, 7.656f, 8.826f},
        new float[] {-10.245f, -8.872f, 6.995f, 8.090f},
        new float[] {-5.787f, -4.024f, 8.396f, 9.292f},
        new float[] {9.160f, 10.132f, -2.647f, -1.675f},
        new float[] {9.227f, 10.126f, -1.789f, -0.890f},
        new float[] {9.112f, 10.108f, -0.997f, -0.001f},
        new float[] {8.967f, 10.165f, -0.103f, 0.949f},
        new float[] {11.193f, 12.432f, 2.132f, 3.218f},
        new float[] {10.325f, 11.564f, 1.529f, 2.615f},
        new float[] {9.446f, 10.685f, 0.919f, 2.005f},
        new float[] {12.085f, 13.940f, 2.688f, 4.236f},
        new float[] {13.522f, 15.734f, 3.864f, 5.987f},
        new float[] {-7.330f, -6.483f, 8.163f, 8.852f},
        new float[] {-6.585f, -5.738f, 8.259f, 8.948f},
        new float[] {-4.187f, -3.112f, 8.760f, 9.979f},
        new float[] {9.172f, 9.993f, -3.517f, -2.600f},
        new float[] {9.172f, 10.001f, -4.325f, -3.410f},
        new float[] {1.900f, 2.459f, 4.168f, 5.337f},
        new float[] {0.055f, 0.614f, -4.479f, -3.311f},
        new float[] {-3.375f, -2.125f, 7.016f, 7.831f},
        new float[] {9.567f, 10.135f, 8.152f, 9.325f},
        new float[] {-7.676f, -7.059f, 2.438f, 3.114f},
        new float[] {-6.520f, -5.903f, 2.438f, 3.114f},
        new float[] {-7.517f, -6.990f, 0.512f, 1.188f},
        new float[] {-6.561f, -6.034f, 0.512f, 1.188f},
        new float[] {3.091f, 5.913f, 5.142f, 7.963f},
        new float[] {-3.104f, -2.428f, -2.618f, -2.001f},
        new float[] {-3.104f, -2.428f, 0.094f, 0.711f},
        new float[] {-3.104f, -2.428f, -1.062f, -0.445f},
        new float[] {-3.457f, -2.835f, -3.712f, -2.913f},
        new float[] {1.279f, 2.179f, -4.498f, -3.598f},
        new float[] {1.279f, 2.179f, -3.714f, -2.814f},
        new float[] {1.289f, 1.911f, -2.860f, -2.061f},
        new float[] {1.425f, 2.380f, -2.131f, -1.175f},
        new float[] {2.161f, 3.901f, -1.472f, -0.650f},
        new float[] {1.763f, 3.502f, -0.597f, 0.225f},
        new float[] {5.877f, 8.422f, 5.203f, 7.747f},
        new float[] {-2.313f, 0.575f, 7.332f, 8.573f},
        new float[] {1.356f, 2.392f, 8.167f, 9.048f},
        new float[] {0.398f, 1.495f, 7.949f, 8.895f},
        new float[] {3.170f, 6.061f, -8.496f, -7.280f},
        new float[] {5.634f, 6.855f, -7.674f, -6.453f},
        new float[] {7.337f, 7.811f, -4.635f, -2.260f},
        new float[] {6.370f, 7.298f, -6.917f, -5.953f},
        new float[] {6.817f, 7.767f, -6.108f, -4.543f},
        new float[] {3.342f, 5.081f, -0.112f, 0.710f},
        new float[] {3.740f, 5.480f, -0.986f, -0.164f},
        new float[] {5.319f, 7.058f, -0.501f, 0.321f},
        new float[] {4.921f, 6.660f, 0.374f, 1.196f},
        new float[] {6.532f, 7.623f, 0.842f, 1.512f},
        new float[] {6.920f, 7.825f, -0.261f, 0.518f},
        new float[] {7.299f, 7.886f, -2.298f, -1.117f},
        new float[] {7.317f, 7.903f, -1.248f, -0.067f},
        new float[] {-5.055f, -4.379f, 6.125f, 6.742f},
        new float[] {-5.055f, -4.379f, 3.413f, 4.030f},
        new float[] {-5.055f, -4.379f, 4.569f, 5.186f},
        new float[] {10.138f, 11.802f, 3.941f, 5.298f},
        new float[] {11.506f, 13.171f, 4.881f, 6.238f},
        new float[] {7.063f, 7.921f, 1.502f, 2.447f},
        new float[] {3.952f, 6.214f, 3.611f, 4.518f},
        new float[] {7.337f, 10.158f, 9.311f, 12.132f},
        new float[] {-9.348f, -7.396f, 3.106f, 6.369f},
        new float[] {-7.093f, -5.843f, 6.121f, 6.936f},
        new float[] {1.558f, 2.457f, 5.460f, 6.358f},
        new float[] {1.558f, 2.457f, 6.327f, 7.226f},
        new float[] {1.558f, 2.457f, 7.176f, 8.074f},
    };

    public CarScript() {
    }

    public override void OnStart()
    {
        transform = GetComponent<TransformComponent>();
    }

    public static Vector3f Lerp(Vector3f a, Vector3f b, float t) {
        return a + (b - a) * t;
    }

    private void ResolveCollisions()
    {
        float px = transform.Position.X;
        float pz = transform.Position.Z;

        for (int i = 0; i < collisionBoxes.Length; i++)
        {
            float minX = collisionBoxes[i][0];
            float maxX = collisionBoxes[i][1];
            float minZ = collisionBoxes[i][2];
            float maxZ = collisionBoxes[i][3];

            if (px > minX && px < maxX && pz > minZ && pz < maxZ)
            {
                // Find minimum penetration axis
                float pushLeft  = px - minX;
                float pushRight = maxX - px;
                float pushBack  = pz - minZ;
                float pushFront = maxZ - pz;

                float minPush = pushLeft;
                int axis = 0; // 0=left, 1=right, 2=back, 3=front

                if (pushRight < minPush) { minPush = pushRight; axis = 1; }
                if (pushBack  < minPush) { minPush = pushBack;  axis = 2; }
                if (pushFront < minPush) { minPush = pushFront; axis = 3; }

                switch (axis)
                {
                    case 0: transform.Position.X = minX; break;
                    case 1: transform.Position.X = maxX; break;
                    case 2: transform.Position.Z = minZ; break;
                    case 3: transform.Position.Z = maxZ; break;
                }

                // Kill speed on collision
                throttle *= 0.5f;
            }
        }
    }

    public override void OnUpdate(float deltaTime)
    {
        float axisX = Input.GetAxis("Horizontal");
        float axisY = Input.GetAxis("Vertical");

        // smooth steering
        if (axisX != 0.0f)
        {
            steer += axisX * steerStep * deltaTime * 60.0f;
        }
        else
        {
            steer *= (float) Math.Pow(0.9f, deltaTime * 60.0f);
        }

        // clamp steering angle
        steer = (float) Math.Clamp(steer, -steerClamp, steerClamp);

        // throttle input
        throttle += axisY * acceleration * deltaTime;
        throttle = (float) Math.Clamp(throttle, minSpeed, maxSpeed);

        // apply drag
        throttle *= (float)Math.Pow(drag, deltaTime * 60.0f);

        // rotate car
        float turnAmount = steer * deltaTime * throttle * 10.0f;
        transform.Rotation.Y -= turnAmount;

        // move car
        Vector3f forward = transform.GetForward();
        transform.Position += forward * throttle;

        // resolve building collisions
        ResolveCollisions();

        // camera follow - compute behind direction from Y rotation only
        float yawRad = transform.Rotation.Y * (float)Math.PI / 180.0f;
        float behindX = (float)Math.Sin(yawRad);
        float behindZ = (float)Math.Cos(yawRad);
        Vector3f target_pos = transform.Position + new Vector3f(behindX, 0, behindZ) * 1.0f + Vector3f.Up * 1.0f;
        CameraTransform.Position = Lerp(CameraTransform.Position, target_pos, 0.1f);
        CameraTransform.Rotation = Lerp(CameraTransform.Rotation, new Vector3f(-15.0f, transform.Rotation.Y, 0.0f), 0.05f);
    }

    public override bool OnKeyPressed(KeyCode keyCode, bool repeat)
    {
        return false;
    }


}
