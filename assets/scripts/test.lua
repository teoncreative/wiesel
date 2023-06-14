require("wiesel.def.lua")
require("input.def.lua")

--- todo move this to input.def.lua
keyCodes = keyCodes or {
    KeyO = 79
}

--- You can define variables inside vars to view them inside the editor
vars = {
    ---@type TransformComponent
    transform = {
        type = "TransformComponent"
    },
    testnumber = 0
}

function OnLoad()
    print('OnLoad')
    vars.transform = GetComponent('TransformComponent')
    vars.transform:SetPosition(0,0,0)
end

function Update(deltaTime)
    print('Update')
    vars.transform:SetPosition(0, vars.testnumber, 0)
    --- todo
    if input.IsPressed(keyCodes.KeyO) then
        if vars.transform then
            vars.transform:Move(0, 10, 0);
        else
            LogInfo("Camera transform not set!")
        end
    end
end
