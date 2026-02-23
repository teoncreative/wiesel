using WieselEngine;
using System;

public class PlayerController : MonoBehavior
{
    private TransformComponent transform;
    private RigidBodyComponent rb;

    public float MoveSpeed = 5.0f;
    public float JumpForce = 8.0f;
    public float RespawnY = -10.0f;

    private bool grounded = false;
    private bool jumped = false;
    private float currentYaw = 0.0f;
    public float TurnSpeed = 720.0f;

    // Coyote time: grace period after leaving a platform where you can still jump
    public float CoyoteTime = 0.12f;
    private float coyoteTimer = 0.0f;

    private Vector3f spawnPosition = new Vector3f(0.0f, 1.0f, 0.0f);

    public PlayerController() {
    }

    public override void OnStart()
    {
        transform = GetComponent<TransformComponent>();
        rb = GetComponent<RigidBodyComponent>();
    }

    public override void OnUpdate(float deltaTime)
    {
        float axisH = Input.GetAxis("Horizontal");
        float axisV = Input.GetAxis("Vertical");

        // Movement: set horizontal velocity, Bullet handles gravity
        float moveX = axisV * MoveSpeed;
        float moveZ = axisH * MoveSpeed;

        Vector3f vel = rb.LinearVelocity;
        rb.LinearVelocity = new Vector3f(moveX, vel.Y, moveZ);

        // Rotate player to face movement direction (smooth)
        if (Mathf.Abs(axisV) > 0.01f || Mathf.Abs(axisH) > 0.01f)
        {
            float targetAngle = Mathf.Atan2(axisV, axisH) * Mathf.Rad2Deg;

            float diff = targetAngle - currentYaw;
            while (diff > 180f) diff -= 360f;
            while (diff < -180f) diff += 360f;

            float maxStep = TurnSpeed * deltaTime;
            if (Mathf.Abs(diff) <= maxStep)
                currentYaw = targetAngle;
            else
                currentYaw += Mathf.Sign(diff) * maxStep;

            transform.Rotation.Y = currentYaw;
        }

        // Ground detection via raycast from feet
        grounded = false;
        float playerX = transform.Position.X;
        float playerY = transform.Position.Y;
        float playerZ = transform.Position.Z;

        Vector3f feetPos = new Vector3f(playerX, playerY + 0.1f, playerZ);
        RaycastHit hit;
        if (Physics.Raycast(ScenePtr, feetPos, Vector3f.Down, 0.4f, out hit, EntityId))
        {
            grounded = true;
        }

        // Coyote time tracking
        if (grounded)
        {
            jumped = false;
            coyoteTimer = CoyoteTime;
        }
        coyoteTimer -= deltaTime;

        // Jump (jumped flag prevents multiple jumps while key is held)
        bool canJump = (grounded || coyoteTimer > 0f) && !jumped;
        if (canJump && Input.GetKey("Jump"))
        {
            vel = rb.LinearVelocity;
            rb.LinearVelocity = new Vector3f(vel.X, JumpForce, vel.Z);
            grounded = false;
            jumped = true;
            coyoteTimer = 0f;
        }

        // Respawn if fallen off
        if (playerY < RespawnY)
        {
            transform.Position = spawnPosition;
            rb.LinearVelocity = new Vector3f(0f, 0f, 0f);
        }
    }
}
