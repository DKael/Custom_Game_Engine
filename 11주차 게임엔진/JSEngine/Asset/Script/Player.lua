local Script = {}
Script.__index = Script

function Script.new(component, properties)

    local self = setmetatable({}, Script)

    self.component = component
    self.owner = component:GetOwner()

    self.moveSpeed = 10.0
    self.mouseSensitivity = 0.15

    return self
end

function Script:BeginPlay()

    local input = Engine.API.Input

    input.SetInputModeGameOnly()
    input.SetCursorLocked(true)
    input.SetCursorVisible(false)

end

function Script:Tick(dt)

    local input = Engine.API.Input

    ------------------------------------------------
    -- Mouse Look
    ------------------------------------------------

    local mouseDelta = input.GetRawMouseDelta()

    local rot = self.owner.Rotation

    rot.z = rot.z + mouseDelta.X * self.mouseSensitivity

    self.owner.Rotation = rot

    ------------------------------------------------
    -- Movement
    ------------------------------------------------

    local move = Vector(0, 0, 0)

    if input.IsRawKeyDown("W") then
        move = move + self.owner:GetActorForwardVector()
    end

    if input.IsRawKeyDown("S") then
        move = move - self.owner:GetActorForwardVector()
    end

    if input.IsRawKeyDown("D") then
        move = move + self.owner:GetActorRightVector()
    end

    if input.IsRawKeyDown("A") then
        move = move - self.owner:GetActorRightVector()
    end

    if move:Size() > 0.001 then
        move = move:Normalized()
    end

    self.owner.Location =
        self.owner.Location + move * self.moveSpeed * dt

end

function Script:EndPlay()

end

return Script