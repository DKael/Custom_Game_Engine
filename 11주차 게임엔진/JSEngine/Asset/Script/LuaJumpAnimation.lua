local LuaJumpAnimation = {}
LuaJumpAnimation.__index = LuaJumpAnimation

function LuaJumpAnimation.new(scriptComponent)
    local self = setmetatable({}, LuaJumpAnimation)
    self.ScriptComponent = scriptComponent
    self.StateMachine = nil
    return self
end

function LuaJumpAnimation:BeginPlay()
    local owner = self.ScriptComponent ~= nil and self.ScriptComponent:GetOwner() or nil
    local skeletalMeshComponent = owner ~= nil and owner:GetSkeletalMeshComponent() or nil
    local animInstance = skeletalMeshComponent ~= nil and skeletalMeshComponent:GetAnimInstance() or nil

    self.StateMachine = animInstance ~= nil and animInstance:GetStateMachine() or nil
end

function LuaJumpAnimation:Tick(deltaTime)
    if self.StateMachine == nil then
        self:BeginPlay()
        if self.StateMachine == nil then
            return
        end
    end

    local input = Engine.API.Input
    if input.IsRawKeyPressed("Space") then
        self.StateMachine:SetParameterTrigger("Jump")
    end
end

return LuaJumpAnimation
