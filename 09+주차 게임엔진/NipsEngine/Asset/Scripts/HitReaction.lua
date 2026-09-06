local tire_reaction = {
    react_to_tag = "Boat",

    scale_pulse = {
        component_class = "StaticMeshComponent",
        duration = 0.22,
        strength = 0.32,
        uniform = true,
    },

    knockback = {
        target = "other",
        speed = 14.0,
        skip_if_target_pushed = true,
    },

    sound = {
        file = "Tire.mp3",
        volume = 1.0,
    },
}

local boat_rock_reaction = {
    react_to_tag = "Rock",
}

local function clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    end
    if value > max_value then
        return max_value
    end
    return value
end

local visual_component = nil
local base_scale = nil
local pulse_time = 0.0
local ResetVignette = nil

local function get_profile(self)
    if self == nil then
        return nil
    end

    if self:GetTag() == "Tire" then
        return tire_reaction
    end

    if self:GetTag() == "Boat" then
        return boat_rock_reaction
    end

    return nil
end

local function resolve_visual_component(self, scale_profile)
    if visual_component ~= nil then
        return visual_component
    end

    local component_class = scale_profile and scale_profile.component_class or "StaticMeshComponent"
    visual_component = self:FindSceneComponentByClass(component_class)

    if visual_component == nil then
        visual_component = self:GetRootComponent()
    end

    if visual_component ~= nil and base_scale == nil then
        base_scale = visual_component:GetRelativeScale()
    end

    return visual_component
end

local function start_scale_pulse(self, hit_profile)
    local scale_profile = hit_profile and hit_profile.scale_pulse
    if scale_profile == nil then
        return
    end

    local visual = resolve_visual_component(self, scale_profile)
    if visual == nil then
        return
    end

    if base_scale == nil then
        base_scale = visual:GetRelativeScale()
    end

    pulse_time = scale_profile.duration or 0.2
end

local function update_scale_pulse(self, delta_time, hit_profile)
    local scale_profile = hit_profile and hit_profile.scale_pulse
    if scale_profile == nil then
        return
    end

    local visual = resolve_visual_component(self, scale_profile)
    if visual == nil or base_scale == nil then
        return
    end

    if pulse_time <= 0.0 then
        visual:SetRelativeScale(base_scale.X, base_scale.Y, base_scale.Z)
        return
    end

    local duration = math.max(0.001, scale_profile.duration or 0.2)
    local strength = scale_profile.strength or 0.25

    pulse_time = math.max(0.0, pulse_time - math.max(0.0, delta_time or 0.0))

    local alpha = 1.0 - clamp(pulse_time / duration, 0.0, 1.0)
    local pulse = math.sin(alpha * math.pi)
    local scale = 1.0 + strength * pulse

    if scale_profile.uniform == false then
        local horizontal = 1.0 + strength * 0.45 * pulse
        local vertical = 1.0 - strength * pulse
        visual:SetRelativeScale(base_scale.X * horizontal, base_scale.Y * horizontal, base_scale.Z * vertical)
        return
    end

    visual:SetRelativeScale(base_scale.X * scale, base_scale.Y * scale, base_scale.Z * scale)
end

local function choose_knockback_target(self, other_actor, knockback_profile)
    local target = knockback_profile and knockback_profile.target or "other"
    if target == "self" then
        return self
    end
    return other_actor
end

local function apply_knockback(self, other_actor, hit_profile)
    local knockback_profile = hit_profile and hit_profile.knockback
    if knockback_profile == nil then
        return
    end

    local target = choose_knockback_target(self, other_actor, knockback_profile)
    if target == nil or not target:IsValid() then
        return
    end

    if knockback_profile.skip_if_target_pushed and IsActorPushed and IsActorPushed(target) then
        return
    end

    local self_pos = self:GetPosition()
    local other_pos = other_actor:GetPosition()
    local from_pos = self_pos
    local to_pos = other_pos

    if target == self then
        from_pos = other_pos
        to_pos = self_pos
    end

    local dx = to_pos.X - from_pos.X
    local dy = to_pos.Y - from_pos.Y
    local length = math.sqrt(dx * dx + dy * dy)

    if length < 0.0001 then
        local forward = target:GetForwardVector()
        dx = -forward.X
        dy = -forward.Y
        length = math.sqrt(dx * dx + dy * dy)
    end

    if length < 0.0001 then
        return
    end

    dx = dx / length
    dy = dy / length

    if ApplyActorKnockback then
        local speed = knockback_profile.speed or 10.0
        ApplyActorKnockback(target, dx * speed, dy * speed, 0.0)
    end
end

local function play_hit_feedback()
    -- HitFeel.HitStop(1.0, 0.0)
    if HitFeel ~= nil and HitFeel.Slomo ~= nil then
        HitFeel.Slomo(0.35, 0.25)
    end

    if Camera ~= nil and Camera.Shake ~= nil then
        Camera.Shake({
            Type = "WaveOscillator",
            Duration = 0.18,
            LocationAmplitude = Vec3.new(0.0, 0.0, 0.0),
            LocationFrequency = Vec3.new(0.0, 0.0, 0.0),
            RotationAmplitude = Vec3.new(4.0, 4.0, 0.0),
            RotationFrequency = Vec3.new(20.0, 20.0, 0.0),
            FOVAmplitude = 0.0,
            FOVFrequency = 0.0,
        })
    end
end

local function play_hit_sound(hit_profile)
    local sound_profile = hit_profile and hit_profile.sound
    if sound_profile == nil or sound_profile.file == nil then
        return
    end

    if Sound == nil or Sound.PlaySFX == nil then
        if Log then Log("[HitReaction] Sound API unavailable") end
        return
    end

    -- 진단: 호출 도달 여부 확인. SoundManager가 엔진 Initialize 시점에만 SFX 폴더를 스캔하므로
    -- 이 로그가 찍히는데도 소리가 안 들리면 엔진을 재시작해야 한다(파일이 맵에 없음).
    if Log then Log("[HitReaction] PlaySFX " .. tostring(sound_profile.file) .. " vol=" .. tostring(sound_profile.volume or 0.5)) end

    Sound.PlaySFX(sound_profile.file, sound_profile.volume or 0.5, false)
end

local function handle_reaction(self, other_actor)
    if other_actor == nil or not other_actor:IsValid() then
        return
    end

    local hit_profile = get_profile(self)
    if hit_profile == nil then
        return
    end

    if hit_profile.react_to_tag ~= nil and other_actor:GetTag() ~= hit_profile.react_to_tag then
        return
    end

    start_scale_pulse(self, hit_profile)
    apply_knockback(self, other_actor, hit_profile)
    play_hit_feedback()
    play_hit_sound(hit_profile)
end

function OnHit(self, other_actor)
    handle_reaction(self, other_actor)
end

function OnOverlapBegin(self, other_actor)
    handle_reaction(self, other_actor)
end

ResetVignette = function()
    Wait(0.18)
    Camera.SetVignette(0.0, 0.75)
end

function OnUpdate(self, delta_time, hitInfo)
    update_scale_pulse(self, delta_time, get_profile(self))
end
