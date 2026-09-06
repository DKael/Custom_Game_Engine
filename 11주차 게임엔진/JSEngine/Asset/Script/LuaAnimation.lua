local LuaAnimation = {}
LuaAnimation.__index = LuaAnimation

local Direction = {
    Idle = 0,
    Forward = 1,
    Left = 2,
    Backward = 3,
    Right = 4,
}

function LuaAnimation.new(animInstance)
    local self = setmetatable({}, LuaAnimation)
    self.AnimInstance = animInstance
    if animInstance ~= nil and animInstance.CreateStateMachine ~= nil then
        self.StateMachine = animInstance:CreateStateMachine()
    end
    return self
end

local function addState(sm, name, path, fallbackIndex)
    if path ~= nil and path ~= "" and sm:AddStateByPath(name, path, true, 1.0) then
        return
    end

    sm:AddStateFromOwnerMesh(name, fallbackIndex, true, 1.0)
end

function LuaAnimation:NativeInitializeAnimation()
    if self.StateMachine == nil then
        return
    end

    local sm = self.StateMachine

    sm:RegisterParameterInt("Direction", Direction.Idle)

    addState(sm, "Idle", "Asset/Animation/Alien Animal/Armature_Attack_Bite.sequence", 0)
    addState(sm, "Forward", "Asset/Animation/Alien Animal/Armature_Attack_Hit.sequence", 1)
    addState(sm, "Left", "Asset/Animation/Alien Animal/Armature_Die_1.sequence", 2)
    addState(sm, "Backward", "Asset/Animation/Alien Animal/Armature_Jump.sequence", 3)
    addState(sm, "Right", "Asset/Animation/Alien Animal/Armature_Rest.sequence", 4)

    sm:SetEntryState("Idle")

    sm:AddIntEqualsTransition("Any", "Idle", "Direction", Direction.Idle, 8.0, 0)
    sm:AddIntEqualsTransition("Any", "Forward", "Direction", Direction.Forward, 8.0, 0)
    sm:AddIntEqualsTransition("Any", "Left", "Direction", Direction.Left, 8.0, 0)
    sm:AddIntEqualsTransition("Any", "Backward", "Direction", Direction.Backward, 8.0, 0)
    sm:AddIntEqualsTransition("Any", "Right", "Direction", Direction.Right, 8.0, 0)
end

function LuaAnimation:NativeUpdateAnimation(deltaTime)
    if self.StateMachine == nil then
        return
    end

    local input = Engine.API.Input
    local direction = Direction.Idle

    if input.IsRawKeyDown("W") then
        direction = Direction.Forward
    elseif input.IsRawKeyDown("A") then
        direction = Direction.Left
    elseif input.IsRawKeyDown("S") then
        direction = Direction.Backward
    elseif input.IsRawKeyDown("D") then
        direction = Direction.Right
    end

    self.StateMachine:SetParameterInt("Direction", direction)
end

return LuaAnimation
