Speed = 120.0

function OnStart(self)
    Log("MoveActor started: " .. self:GetName())
end

function OnUpdate(self, deltaTime)
    self:AddPosition(0.0, 0.0, Speed * deltaTime)
end
