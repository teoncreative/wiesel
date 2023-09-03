using WieselEngine;

public class TestBehavior : MonoBehavior
{

    private TransformComponent transform;
    private ulong a;

    public TestBehavior() {
        EngineInternal.LogInfo("TestBehavior!");
    }

    public void OnStart()
    {
        EngineInternal.LogInfo("Start!");
        transform = GetComponent<TransformComponent>();
        a = 0;
        b = 0;
        transform.Position.X = 0.0f;
    }

    public void OnUpdate(float deltaTime)
    {
        EngineInternal.LogInfo("Update! " + a + " " + b);
        a += 1;
        b += 1;
        transform.Position.X += 0.1f;
    }

}
