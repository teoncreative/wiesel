using WieselEngine;

public class PlayerScript : MonoBehavior
{
    public float moveSpeed = 5f;
    public float mouseSensitivity = 2f;
    public float jumpForce = 8f;

    TransformComponent transform;
    TransformComponent cameraTransform;
    RigidBodyComponent rigidBody;
    public float camRotX = 0;
    public float camRotY = 0;
    bool grounded = false;

    public override void OnStart()
    {
        transform = GetComponent<TransformComponent>();
        cameraTransform = Entity.GetChild(0).GetComponent<TransformComponent>();
        rigidBody = GetComponent<RigidBodyComponent>();
    }

    public override void OnUpdate(float deltaTime)
    {
        Move(deltaTime);
        Look();
    }

    void Move(float deltaTime)
    {
        float x = 0;
        float z = 0;

        if (Input.GetKey("Left")) x = -1;
        if (Input.GetKey("Right")) x = 1;
        if (Input.GetKey("Up")) z = 1;
        if (Input.GetKey("Down")) z = -1;

        Vector3f move = new Vector3f(x, 0, z);
        Vector3f finalMove = move * moveSpeed * deltaTime;
        transform.Translate(finalMove);

        if (Input.GetKey("Jump") && grounded)
        {
            rigidBody.AddImpulse(new Vector3f(0, jumpForce, 0));
            grounded = false;
        }
    }

    public override void OnCollisionEnter(ulong otherEntity)
    {
        grounded = true;
    }

    void Look()
    {
        float mouseX = Input.GetAxis("Mouse X");
        float mouseY = Input.GetAxis("Mouse Y");

        // Accumulate rotation from per-frame deltas
        camRotY -= mouseX * mouseSensitivity;
        camRotX -= mouseY * mouseSensitivity;
        camRotX = Mathf.Clamp(camRotX, -89f, 89f);

        // Rotate player left/right
        transform.Rotation = new Vector3f(0, camRotY, 0);

        // Rotate camera up/down
        cameraTransform.Rotation = new Vector3f(camRotX, 0, 0);
    }

    public override bool OnKeyPressed(KeyCode keyCode, bool repeat)
    {
        if (keyCode == KeyCode.Escape)
        {
            if (Input.GetCursorMode() == CursorMode.Relative)
            {
                Input.SetCursorMode(CursorMode.Normal);
            }
            else
            {
                Input.SetCursorMode(CursorMode.Relative);
            }
        }
        return false;
    }
}











