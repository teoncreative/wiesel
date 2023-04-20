require("wiesel.def.lua")
require("input.def.lua")

---@type TransformComponent
local transform

function Start()
    print('Start')
    transform = GetComponent('TransformComponent')
end

function Update(deltaTime)
---    print(transform.position.x);
    transform:Move(0, -10 * deltaTime, 0)
    if input.GetKey('Up') then
        print('Up key pressed')
    end
end