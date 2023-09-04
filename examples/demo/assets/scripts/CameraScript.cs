using WieselEngine;

public class CameraScript : MonoBehavior
{

    private TransformComponent transform;
    public float CameraMoveSpeed = 8.0f;

    public CameraScript() {
    }

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
        float inputX = Input.GetAxis("Mouse X");
        float inputY = Input.GetAxis("Mouse Y");
        transform.Rotation = new Vector3f(inputY, inputX, 0.0f);
    }

}
