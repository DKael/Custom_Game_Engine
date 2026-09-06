function OnStart(self)
    Log("DestroyOnOverlap ready: " .. self:GetName())
end

function OnOverlapBegin(self, otherActor)
    if otherActor and otherActor:IsValid() then
        Log(self:GetName() .. " overlapped with " .. otherActor:GetName())
    end
    self:Destroy()
end
