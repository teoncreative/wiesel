using WieselEngine;

public class TestBehavior : MonoBehavior
{

    private TransformComponent transform;

    public TestBehavior() {
    }

    public void Start()
    {
        EngineInternal.LogInfo("Start!");
        transform = GetComponent<TransformComponent>();
    }

    public void Update()
    {
        EngineInternal.LogInfo($"{transform.Position.X}");
        transform.Position.X += 0.1f;
    }

}
