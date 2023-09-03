using WieselEngine;

public class TestBehavior : MonoBehavior
{

    private TransformComponent transform;
    public float Speed = 0.1f;

    public TestBehavior() {
        EngineInternal.LogInfo("TestBehavior!");
    }

    public void OnStart()
    {
        EngineInternal.LogInfo("Start!");
        transform = GetComponent<TransformComponent>();
        transform.Position.X = 0.0f;
    }

    public void OnUpdate(float deltaTime)
    {
        EngineInternal.LogInfo("Update: " + Value8);
        transform.Position.X += Speed;
    }

}
