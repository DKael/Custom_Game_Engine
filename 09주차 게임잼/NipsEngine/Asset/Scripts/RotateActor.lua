RotationSpeed = 90.0

function OnStart(self)
    Log("RotateActor started: " .. self:GetName())
end

function OnUpdate(self, deltaTime)
    local rotation = self:GetRotation()
    self:SetRotation(rotation.X, rotation.Y, rotation.Z + RotationSpeed * deltaTime)
end
