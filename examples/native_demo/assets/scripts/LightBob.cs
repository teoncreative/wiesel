using WieselEngine;
using System;

public class LightBob : MonoBehavior
{
    private TransformComponent transform;
    private float startX;
    private float startY;
    private float startZ;
    private float time;

    public float Speed = 0.6f;
    public float PhaseOffset = 0.0f;
    public Entity handle;

    public LightBob() {
    }

    public override void OnStart()
    {
        transform = GetComponent<TransformComponent>();
        startX = transform.Position.X;
        startY = transform.Position.Y;
        startZ = transform.Position.Z;
        time = 0.0f;

        // Each light gets a different phase based on position
        PhaseOffset = startX * 0.9f;
    }

    public override void OnUpdate(float deltaTime)
    {
        time += deltaTime;
        float t = time * Speed + PhaseOffset;

        // Large X sweep so lights cross through each other near the center (x=5)
        float x = 5.0f + (float)Math.Sin(t * 0.7f) * 4.0f;
        float y = startY + 0.5f + (float)Math.Sin(t * 0.5f + 1.2f) * 0.5f;
        float z = startZ + (float)Math.Sin(t * 0.4f + 2.5f) * 1.5f;

        transform.Position = new Vector3f(x, y, z);
    }
}