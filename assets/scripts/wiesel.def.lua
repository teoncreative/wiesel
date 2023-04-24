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

---@param dx number
---@param dy number
---@param dz number
---@overload fun(delta: Vec3)
function TransformComponent:Move(dx, dy, dz) end

---@param x number
---@param y number
---@param z number
---@overload fun(pos: Vec3)
function TransformComponent:SetPosition(x, y, z) end

---@param dx number
---@param dy number
---@param dz number
---@overload fun(delta: Vec3)
function TransformComponent:Rotate(dx, dy, dz) end

---@param x number
---@param y number
---@param z number
---@overload fun(rot: Vec3)
function TransformComponent:SetRotation(x, y, z) end


---@param dx number
---@param dy number
---@param dz number
---@overload fun(delta: Vec3)
function TransformComponent:Resize(dx, dy, dz) end

---@param x number
---@param y number
---@param z number
---@overload fun(scale: Vec3)
function TransformComponent:SetScale(x, y, z) end

-- todo other transform functions

---@generic Component
---@param name string
---@return Component
function GetComponent(name) end

--- Log something to the console with debug tag.
---@param msg string
function LogDebug(msg) end

--- Log something to the console with info tag.
---@param msg string
function LogInfo(msg) end

--- Log something to the console with warn tag.
---@param msg string
function LogWarn(msg) end

--- Log something to the console with error tag.
---@param msg string
function LogError(msg) end
