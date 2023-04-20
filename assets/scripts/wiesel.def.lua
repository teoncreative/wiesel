---@class Vec3
---@field x number
---@field y number
---@field z number
local Vec3 = {
    x = 0.0,
    y = 0.0,
    z = 0.0
}

---@class TransformComponent
---@field position Vec3
---@field rotation Vec3
---@field scale Vec3
TransformComponent = {
    position = {},
    rotation = {},
    scale = {}
}
---@param x number
---@param y number
---@param z number
function TransformComponent:Move(x, y, z) end

---@generic Component
---@param class `Component`
---@return Component
function GetComponent(name) end
