require("wiesel.def.lua")
require("input.def.lua")

---@type TransformComponent
local transform

--- You can define variables inside vars to view them inside the editor
vars = {
}

function OnLoad()
    print('OnLoad')
    transform = GetComponent('TransformComponent')
    transform:SetPosition(0,0,0)
end

function Update(deltaTime)
end
