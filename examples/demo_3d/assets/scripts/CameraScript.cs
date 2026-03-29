using WieselEngine;

public class CameraScript : MonoBehavior
{
    private TransformComponent transform;
    public float CameraMoveSpeed = 8.0f;
    public float MouseSensitivity = 2.0f;

    private float rotX = 0.0f;
    private float rotY = 0.0f;

    public override void OnStart()
    {
        transform = GetComponent<TransformComponent>();
    }

    public override void OnUpdate(float deltaTime)
    {
        float axisX = Input.GetAxis("Horizontal");
        float axisY = Input.GetAxis("Vertical");
        transform.Position += transform.GetForward() * deltaTime * CameraMoveSpeed * axisY;
        transform.Position += transform.GetRight() * deltaTime * CameraMoveSpeed * axisX;

        float mouseX = Input.GetAxis("Mouse X");
        float mouseY = Input.GetAxis("Mouse Y");
        rotY += mouseX * MouseSensitivity;
        rotX += mouseY * MouseSensitivity;
        rotX = Mathf.Clamp(rotX, -89.0f, 89.0f);
        transform.Rotation = new Vector3f(rotX, rotY, 0.0f);
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