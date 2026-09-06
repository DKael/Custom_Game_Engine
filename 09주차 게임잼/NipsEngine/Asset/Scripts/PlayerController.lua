-- 적재물이 없을 때의 기준 질량입니다. 단위: mass
local base_mass = 1.0
-- 정규화된 적재 무게가 질량에 얼마나 강하게 반영될지 정하는 배수입니다. 단위: ratio
local cargo_mass_scale = 2.0

-- 마우스 이동량 대비 카메라 회전 민감도입니다. 단위: rad/pixel
local look_speed = 0.001
-- 카메라가 아래로 내려갈 수 있는 최소 각도입니다. 단위: rad
local min_pitch = 0
-- 카메라가 위로 올라갈 수 있는 최대 각도입니다. 단위: rad
local max_pitch = 1.4

-- 전진 입력 시 적용되는 기본 가속도입니다. 단위: unit/s^2
local boat_forward_accel = 20.0
-- 후진 상태에서 뒤로 붙는 기본 가속도입니다. 단위: unit/s^2
local boat_reverse_accel = 15.0
-- 전진 중 후진 입력을 넣었을 때 감속하는 제동 가속도입니다. 단위: unit/s^2
local boat_brake_accel = 15.0
-- 입력이 없을 때 전후진 속도를 0으로 되돌리는 감쇠량입니다. 단위: unit/s^2
local boat_linear_drag = 10.0
-- 조향 입력 시 yaw 속도를 누적시키는 기본 회전 가속도입니다. 단위: deg/s^2
local boat_turn_accel = 60.0
-- 조향 입력이 없을 때 회전 속도를 줄이는 감쇠량입니다. 단위: deg/s^2
local boat_turn_drag = 30.0
-- 가벼운 기준 상태에서의 최대 전진 속도입니다. 단위: unit/s
local boat_max_forward_speed = 20.0
-- 가벼운 기준 상태에서의 최대 후진 속도입니다. 단위: unit/s
local boat_max_reverse_speed = 10.0
-- 가벼운 기준 상태에서의 최대 회전 속도입니다. 단위: deg/s
local boat_max_yaw_speed = 60.0
-- 저속에서도 남겨둘 최소 조향력 비율입니다. 단위: ratio(0~1)
local boat_min_steer_authority = 0.2
-- 이 값보다 작은 속도는 0으로 정리하는 임계값입니다. 단위: speed
local boat_speed_epsilon = 0.001

local orbit_yaw = nil
local orbit_pitch = nil
local orbit_radius = nil

local function clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    end

    if value > max_value then
        return max_value
    end

    return value
end

local function get_cargo_weight()
    if GetDriftSalvageWeight then
        return math.max(0.0, GetDriftSalvageWeight())
    end

    return 0.0
end

local function get_cargo_weight_capacity()
    if GetDriftSalvageWeightCapacity then
        return math.max(0.001, GetDriftSalvageWeightCapacity())
    end

    return 1.0
end

local function get_cargo_weight_ratio()
    return clamp(get_cargo_weight() / get_cargo_weight_capacity(), 0.0, 1.0)
end

local function get_effective_mass()
    return math.max(1.0, base_mass + get_cargo_weight_ratio() * cargo_mass_scale)
end

local function get_components()
    if Pawn == nil then
        return nil, nil, nil
    end

    return Pawn:GetRootComponent(), Pawn:GetCharacterComponent(), Pawn:GetCameraComponent()
end

local function is_input_locked()
    if Pawn == nil or IsActorPushed == nil then
        return false
    end

    return IsActorPushed(Pawn)
end

local function get_input_axis(negative_key, positive_key)
    local value = 0.0
    if Input.GetKey(negative_key) then
        value = value - 1.0
    end
    if Input.GetKey(positive_key) then
        value = value + 1.0
    end
    return value
end

function OnUpdate(delta_time)
    if is_input_locked() then
        return
    end

    local root, mesh, camera = get_components()
    if root == nil or mesh == nil or Pawn == nil then
        return
    end

    -- local dt = clamp(delta_time or 0.0, 0.0001, 0.05)
    local throttle_input = get_input_axis("S", "W")
    local steer_input = get_input_axis("A", "D")
    local mass = get_effective_mass()

    Pawn:UpdateBoatMovement(
        delta_time,
        throttle_input,
        steer_input,
        mass,
        boat_forward_accel,
        boat_reverse_accel,
        boat_brake_accel,
        boat_linear_drag,
        boat_turn_accel,
        boat_turn_drag,
        boat_max_forward_speed,
        boat_max_reverse_speed,
        boat_max_yaw_speed,
        boat_min_steer_authority,
        boat_speed_epsilon
    )
end

function OnKeyDown(key)
end

function OnKeyUp(key)
end

function OnMouseMove(delta_x, delta_y, mouse_x, mouse_y)
    local root, mesh, camera = get_components()
    if root == nil or camera == nil then
        return
    end

    local camera_offset = camera:GetRelativeLocation()
    if orbit_radius == nil or orbit_yaw == nil or orbit_pitch == nil then
        orbit_radius = math.sqrt(
            camera_offset.X * camera_offset.X +
            camera_offset.Y * camera_offset.Y +
            camera_offset.Z * camera_offset.Z
        )
        if orbit_radius < 0.0001 then
            orbit_radius = 1.0
        end

        orbit_yaw = math.atan(camera_offset.Y, camera_offset.X)
        local flat_length = math.sqrt(camera_offset.X * camera_offset.X + camera_offset.Y * camera_offset.Y)
        orbit_pitch = math.atan(camera_offset.Z, flat_length)
    end

    orbit_yaw = orbit_yaw + delta_x * look_speed
    orbit_pitch = math.max(min_pitch, math.min(max_pitch, orbit_pitch + delta_y * look_speed))

    local cos_pitch = math.cos(orbit_pitch)
    local sin_pitch = math.sin(orbit_pitch)
    local cos_yaw = math.cos(orbit_yaw)
    local sin_yaw = math.sin(orbit_yaw)

    local next_x = orbit_radius * cos_pitch * cos_yaw
    local next_y = orbit_radius * cos_pitch * sin_yaw
    local next_z = orbit_radius * sin_pitch

    camera:SetRelativeLocation(next_x, next_y, next_z)

    local pivot = root:GetWorldLocation()
    camera:LookAt(pivot.X, pivot.Y, pivot.Z)
end

function OnMouseClick(button, is_pressed, mouse_x, mouse_y)
end
