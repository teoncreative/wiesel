using WieselEngine;
using System;

public class CameraFollow : MonoBehavior
{
    private TransformComponent transform;

    // Attached from C++ via AttachExternComponent
    public TransformComponent PlayerTransform;

    // Camera offset behind and above the player
    public float OffsetX = -8.0f;
    public float OffsetY = 5.0f;
    public float OffsetZ = 0.0f;

    // Smoothing factor (higher = more responsive)
    public float SmoothSpeed = 5.0f;

    public CameraFollow() {
    }

    public override void OnStart()
    {
        transform = GetComponent<TransformComponent>();
    }

    public override void OnUpdate(float deltaTime)
    {
        if (PlayerTransform == null) return;

        // Target position: player position + offset
        float targetX = PlayerTransform.Position.X + OffsetX;
        float targetY = PlayerTransform.Position.Y + OffsetY;
        float targetZ = PlayerTransform.Position.Z + OffsetZ;

        // Smooth interpolation
        float t = Mathf.Clamp01(SmoothSpeed * deltaTime);
        float camX = Mathf.Lerp(transform.Position.X, targetX, t);
        float camY = Mathf.Lerp(transform.Position.Y, targetY, t);
        float camZ = Mathf.Lerp(transform.Position.Z, targetZ, t);

        transform.Position.X = camX;
        transform.Position.Y = camY;
        transform.Position.Z = camZ;

        // Compute look-at direction from camera to player
        float dx = PlayerTransform.Position.X - camX;
        float dy = PlayerTransform.Position.Y - camY;
        float dz = PlayerTransform.Position.Z - camZ;

        float horizontalDist = Mathf.Sqrt(dx * dx + dz * dz);

        // Pitch: positive = look up, negative = look down
        // GLM quat(vec3) uses X=pitch, Y=yaw, Z=roll, applied as Ry * Rx * Rz
        float pitch = Mathf.Atan2(dy, horizontalDist) * Mathf.Rad2Deg;

        // Yaw: default forward is -Z
        // Forward after yaw = (-sin(yaw), 0, -cos(yaw))
        // We want forward horizontal ∝ (dx, 0, dz), so yaw = atan2(-dx, -dz)
        float yaw = Mathf.Atan2(-dx, -dz) * Mathf.Rad2Deg;

        transform.Rotation = new Vector3f(pitch, yaw, 0.0f);
    }
}