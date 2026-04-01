using WieselEngine;

public class PlayerScript : MonoBehavior
{
    public float moveSpeed = 5f;
    public float mouseSensitivity = 2f;
    public float jumpForce = 8f;

    TransformComponent transform;
    TransformComponent cameraTransform;
    RigidBodyComponent rigidBody;
    UIDocumentComponent hud;
    public float camRotX = 0;
    public float camRotY = 0;
    bool grounded = false;

    // Stamina system: 4 charges, recharges one at a time (lowest empty first)
    const int maxCharges = 4;
    const float rechargeTime = 3.0f; // seconds per charge
    int readyCharges = 4;
    float rechargingTimer = 0; // timer for the charge currently recharging

    public override void OnStart()
    {
        transform = GetComponent<TransformComponent>();
        cameraTransform = Entity.GetChild(0).GetComponent<TransformComponent>();
        rigidBody = GetComponent<RigidBodyComponent>();

        Console.Log("On Start called");
        Entity hudEntity = FindEntity("HUD");
        if (hudEntity != null)
        {
            hud = hudEntity.GetComponent<UIDocumentComponent>();
        }
        Console.Log("hudEntity: " + (hudEntity != null));
        Console.Log("hud: " + (hud != null));
    }

    public override void OnUpdate(float deltaTime)
    {
        Move(deltaTime);
        Look();
        UpdateStamina(deltaTime);
    }

    void UpdateStamina(float deltaTime)
    {
        // Recharge one bar at a time - the lowest empty one
        if (readyCharges < maxCharges)
        {
            rechargingTimer += deltaTime;
            if (rechargingTimer >= rechargeTime)
            {
                readyCharges++;
                rechargingTimer = 0;
            }
        }

        // Push state to HUD
        if (hud != null)
        {
            for (int i = 0; i < maxCharges; i++)
            {
                if (i < readyCharges)
                {
                    // Full bar (ready) - 100%
                    hud.SetInt("stamina_" + i, 100);
                }
                else if (i == readyCharges)
                {
                    // Currently recharging bar - show progress
                    int fill = (int)(rechargingTimer / rechargeTime * 100);
                    hud.SetInt("stamina_" + i, fill);
                }
                else
                {
                    // Empty bar - 0%
                    hud.SetInt("stamina_" + i, 0);
                }
            }
        }
    }

    bool ConsumeCharge()
    {
        if (readyCharges > 0)
        {
            readyCharges--;
            // If we just emptied a full bar, reset recharge timer
            // so the newly empty bar starts recharging from 0
            if (readyCharges < maxCharges && rechargingTimer == 0)
            {
                rechargingTimer = 0;
            }
            return true;
        }
        return false;
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
            if (ConsumeCharge())
            {
                rigidBody.AddImpulse(new Vector3f(0, jumpForce, 0));
                grounded = false;
            }
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

        camRotY -= mouseX * mouseSensitivity;
        camRotX -= mouseY * mouseSensitivity;
        camRotX = Mathf.Clamp(camRotX, -89f, 89f);

        transform.Rotation = new Vector3f(0, camRotY, 0);
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
