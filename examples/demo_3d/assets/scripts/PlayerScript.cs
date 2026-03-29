using WieselEngine;

public class PlayerScript : MonoBehavior
{
    public float moveSpeed = 5f;
    public float mouseSensitivity = 2f;

    TransformComponent transform;
    public Entity camera;
    TransformComponent cameraTransform;
    float camRotX = 0;

    public void Start()
    {
        transform = GetComponent<TransformComponent>();
        cameraTransform = camera.GetComponent<TransformComponent>();
    }

    public void Update()
    {
        Move();
        Look();
    }

    void Move()
    {
        float x = 0;
        float z = 0;

        if (Input.GetKey("A")) x = -1;
        if (Input.GetKey("D")) x = 1;
        if (Input.GetKey("W")) z = 1;
        if (Input.GetKey("S")) z = -1;

        Vector3f move = new Vector3f(x, 0, z);
        transform.Translate(move * moveSpeed * Time.DeltaTime);
    }

    void Look()
    {
        float mouseX = Input.GetAxis("Mouse X");
        float mouseY = Input.GetAxis("Mouse Y");

        // rotate player left/right
        transform.Rotation.Y += mouseX * mouseSensitivity;

        // rotate camera up/down
        camRotX -= mouseY * mouseSensitivity;
        cameraTransform.Rotation = new Vector3f(camRotX, 0, 0);
    }
}

