using System;
using WieselEngine;

public class CarScript : MonoBehavior
{

    private TransformComponent transform;
    public TransformComponent CameraTransform;

    public float steer = 0.0f;
    public float throttle = 0.0f;

    public float steerStep = 0.5f;        // controls how fast steering changes
    public float acceleration = 0.08f;      // rate of speed gain
    public float maxSpeed = 0.5f;          // forward speed limit
    public float minSpeed = -0.2f;         // reverse speed limit
    public float drag = 0.998f;             // natural slowdown
    public float steerClamp = 30.0f;       // in degrees

    public CarScript() {
    }

    public override void OnStart()
    {
        transform = GetComponent<TransformComponent>();
    }

    public static Vector3f Lerp(Vector3f a, Vector3f b, float t) {
        return a + (b - a) * t;
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
        steer = (float) Math.Clamp(steer, -steerClamp, steerClamp); // degrees

        // throttle input
        throttle += axisY * acceleration * deltaTime;
        throttle = (float) Math.Clamp(throttle, minSpeed, maxSpeed); // reverse to forward speed

        // apply drag
        throttle *= (float)Math.Pow(drag, deltaTime * 60.0f);

        // rotate car
        float turnAmount = steer * deltaTime * throttle * 10.0f;
        transform.Rotation.Y -= turnAmount;

        // move car
        Vector3f forward = transform.GetForward();
        transform.Position -= forward * throttle;

        // camera follow
        Vector3f offset = transform.GetForward() * 6.0f + Vector3f.Up * 0.15f;
        CameraTransform.Position = Lerp(CameraTransform.Position, transform.Position + offset, 0.1f);
        CameraTransform.Rotation = Lerp(CameraTransform.Rotation, new Vector3f(transform.Rotation.X, transform.Rotation.Y + 180.0f, transform.Rotation.Z), 0.05f);
    }

    public override bool OnKeyPressed(KeyCode keyCode, bool repeat)
    {
        return false;
    }


}