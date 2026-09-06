local move_speed = 1.0
local look_speed = 1.0

function OnKeyDown(key)
    if Camera == nil then return end

    local forward = Camera:GetForwardVector()
    local right = Camera:GetRightVector()
    local up = Camera:GetUpVector()

    if key == "W" then Camera:AddPosition(forward.X * move_speed, forward.Y * move_speed, forward.Z * move_speed) end
    if key == "S" then Camera:AddPosition(-forward.X * move_speed, -forward.Y * move_speed, -forward.Z * move_speed) end
    if key == "D" then Camera:AddPosition(right.X * move_speed, right.Y * move_speed, right.Z * move_speed) end
    if key == "A" then Camera:AddPosition(-right.X * move_speed, -right.Y * move_speed, -right.Z * move_speed) end
    if key == "E" then Camera:AddPosition(up.X * move_speed, up.Y * move_speed, up.Z * move_speed) end
    if key == "Q" then Camera:AddPosition(-up.X * move_speed, -up.Y * move_speed, -up.Z * move_speed) end
end

function OnKeyUp(key)
end


function OnMouseMove(delta_x, delta_y, mouse_x, mouse_y)
    if Camera == nil then return end
    if IsMouseLocked ~= nil and not IsMouseLocked() then return end
    
    local rotation = Camera:GetRotation()
    Camera:SetRotation(rotation.X - delta_y * look_speed, rotation.Y + delta_x * look_speed, rotation.Z)
end

function OnMouseClick(button, is_pressed, mouse_x, mouse_y)
end

function OnUpdate()
end