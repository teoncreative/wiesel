using WieselEngine;

public class CoinSpin : MonoBehavior
{
    private TransformComponent transform;
    public float SpinSpeed = 180.0f;
    private bool collected = false;

    public CoinSpin() {
    }

    public override void OnStart()
    {
        transform = GetComponent<TransformComponent>();
    }

    public override void OnUpdate(float deltaTime)
    {
        if (collected) return;
        transform.Rotation.Y += SpinSpeed * deltaTime;
    }

    public override void OnTriggerEnter(ulong otherEntityId)
    {
        if (collected) return;
        collected = true;
        Internals.Log_Info("Coin collected!");

        ModelComponent model = GetComponent<ModelComponent>();
        if (model != null)
        {
            model.EnableRendering = false;
        }
    }
}
