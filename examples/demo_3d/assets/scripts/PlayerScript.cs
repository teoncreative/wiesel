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
    UIDocumentComponent settings;
    public float camRotX = 0;
    public float camRotY = 0;
    bool grounded = false;
    bool settingsOpen = false;

    // Stamina system: 4 charges, recharges one at a time (lowest empty first)
    const int maxCharges = 4;
    const float rechargeTime = 3.0f;
    int readyCharges = 4;
    float rechargingTimer = 0;

    public override void OnStart()
    {
        transform = GetComponent<TransformComponent>();
        cameraTransform = Entity.GetChild(0).GetComponent<TransformComponent>();
        rigidBody = GetComponent<RigidBodyComponent>();

        Entity hudEntity = FindEntity("HUD");
        if (hudEntity != null)
        {
            hud = hudEntity.GetComponent<UIDocumentComponent>();
        }

        Entity settingsEntity = FindEntity("Settings");
        if (settingsEntity != null)
        {
            settings = settingsEntity.GetComponent<UIDocumentComponent>();
            settings.Visible = false;
        }

        Input.SetCursorMode(CursorMode.Relative);
    }

    public override void OnUpdate(float deltaTime)
    {
        if (!settingsOpen)
        {
            Move(deltaTime);
            Look();
        }
        UpdateStamina(deltaTime);
    }

    void ToggleSettings()
    {
        settingsOpen = !settingsOpen;
        if (settings != null)
        {
            settings.Visible = settingsOpen;
        }
        if (settingsOpen)
        {
            Input.SetCursorMode(CursorMode.Normal);
        }
        else
        {
            Input.SetCursorMode(CursorMode.Relative);
        }
    }

    void UpdateStamina(float deltaTime)
    {
        if (readyCharges < maxCharges)
        {
            rechargingTimer += deltaTime;
            if (rechargingTimer >= rechargeTime)
            {
                readyCharges++;
                rechargingTimer = 0;
            }
        }

        if (hud != null)
        {
            for (int i = 0; i < maxCharges; i++)
            {
                if (i < readyCharges)
                {
                    hud.SetInt("stamina_" + i, 100);
                }
                else if (i == readyCharges)
                {
                    int fill = (int)(rechargingTimer / rechargeTime * 100);
                    hud.SetInt("stamina_" + i, fill);
                }
                else
                {
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

    public override void OnCollisionEnter(Entity other)
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
            ToggleSettings();
            return true;
        }
        return false;
    }
}
