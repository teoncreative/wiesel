require("wiesel.def.lua")
require("input.def.lua")

--- todo move this to input.def.lua
keyCodes = keyCodes or {
    KeyO = 79
}

---@type TransformComponent
local transform

--- You can define variables inside vars to view them inside the editor
vars = {
    ---@type TransformComponent
    cameraTransform = {
        type = "TransformComponent"
    },
    testnumber = 0
}

function OnLoad()
    print('OnLoad')
    transform = GetComponent('TransformComponent')
    transform:SetPosition(0,0,0)
end

function Update(deltaTime)
    if input.IsPressed(keyCodes.KeyO) then
        if vars.cameraTransform then
            vars.cameraTransform:Move(0, 10, 0);
        else
            LogInfo("Camera transform not set!")
        end
    end
end
