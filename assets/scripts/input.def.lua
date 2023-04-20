input = {}

---
--- This function uses mapped inputs, for example 'Up' can be mapped to ArrowUp and W keys
--- and this function will return true if any of those buttons are pressed.
---@param name string Input name
---@return boolean Is input pressed
function input.GetKey(name) end

---
---@param name string Axis name
---@return number Value
function input.GetAxis(name) end

---
---@param keycode number Keycode
---@return boolean Is key pressed
function input.IsPressed(keycode) end