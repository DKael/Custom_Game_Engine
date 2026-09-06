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
local boat_linear_drag = 5.0
-- 가벼운 기준 상태에서의 최대 전진 속도입니다. 단위: unit/s
local boat_max_forward_speed = 40.0
-- 가벼운 기준 상태에서의 최대 후진 속도입니다. 단위: unit/s
local boat_max_reverse_speed = 20.0

-- 최대 속도에서 적용되는 조향 가속도입니다. 단위: deg/s^2
local boat_max_turn_accel = 45.0
-- 조향 입력이 없을 때 회전 속도를 줄이는 감쇠량입니다. 단위: deg/s^2
local boat_turn_damping = 10.0
-- 최대 전진 속도에서의 최대 회전 속도입니다. 단위: deg/s
local boat_max_turn_speed = 60.0
-- 정지 상태에서의 최소 조향 가속도입니다. 단위: deg/s^2
local boat_min_turn_accel = 12.0
local boat_forward_loop_sfx = "boatMove.mp3"
local boat_forward_loop_volume = 0.35
local boat_forward_loop_stop_speed = 0.05

local orbit_yaw = nil
local orbit_pitch = nil
local orbit_radius = nil
local is_forward_pressed = false
local is_backward_pressed = false
local is_turn_left_pressed = false
local is_turn_right_pressed = false
local boat_forward_speed = 0.0
local boat_turn_speed = 0.0
local active_routines = {} -- 진행 중인 Coroutine 목록
local is_slomo_input_pressed = false
local is_boat_slomo_running = false -- Slomo Coroutine이 중복 실행되지 않게 막기용
local is_forward_loop_sfx_playing = false
local boat_slomo_time_scale = 0.35
local boat_slomo_refresh_duration = 0.08
local boat_slomo_remaining_time = 0.0

local function clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    end

    if value > max_value then
        return max_value
    end

    return value
end

local function resume_routine(routine)
    local ok, wait_seconds = coroutine.resume(routine.thread)

    if not ok then
        if Warning ~= nil then
            Warning("Boat slomo coroutine error: " .. tostring(wait_seconds))
        end
        return false
    end

    if coroutine.status(routine.thread) == "dead" then
        return false
    end

    routine.wait_remaining = math.max(0.0, tonumber(wait_seconds) or 0.0)
    return true
end

local function start_routine(func)
    local routine = {
        thread = coroutine.create(func),
        wait_remaining = 0.0,
    }

    if resume_routine(routine) then
        table.insert(active_routines, routine)
    end
end

local function tick_routines(delta_time)
    for i = #active_routines, 1, -1 do
        local routine = active_routines[i]

        if routine.wait_remaining > 0.0 then
            routine.wait_remaining = math.max(
                0.0,
                routine.wait_remaining - math.max(0.0, delta_time or 0.0)
            )
        end

        if routine.wait_remaining <= 0.0 then
            if not resume_routine(routine) then
                table.remove(active_routines, i)
            end
        end
    end
end

local function boat_slomo_visual_routine()
    if is_boat_slomo_running then
        return
    end

    is_boat_slomo_running = true

--     if Camera ~= nil and Camera.FOVKick ~= nil then
--         Camera.FOVKick(6.0, 0.2)
--     end

--     Wait(0.12)

    is_boat_slomo_running = false
end

local function move_toward(current, target, max_delta)
    if current < target then
        return math.min(current + max_delta, target)
    end

    if current > target then
        return math.max(current - max_delta, target)
    end

    return target
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

local function update_forward_loop_sfx(should_play)
    if not Sound or not Sound.PlaySFX or not Sound.StopSFX then
        return
    end

    if should_play and not is_forward_loop_sfx_playing then
        Sound.PlaySFX(boat_forward_loop_sfx, boat_forward_loop_volume, true)
        is_forward_loop_sfx_playing = true
    elseif not should_play and is_forward_loop_sfx_playing then
        Sound.StopSFX(boat_forward_loop_sfx)
        is_forward_loop_sfx_playing = false
    end
end

-- TODO: 이 함수 내용들은 PlayerController 스크립트보다는 별도의 Pawn 스크립트에 들어가는 게 더 적절할 수 있습니다.
function OnUpdate(delta_time)
    local physics_delta_time = delta_time or 0.0
    local routine_delta_time = physics_delta_time
    if TimeManager ~= nil and TimeManager.GetUnscaledDeltaTime ~= nil then
        routine_delta_time = TimeManager.GetUnscaledDeltaTime()
    end
    local input_delta_time = routine_delta_time

    tick_routines(routine_delta_time)
    if is_slomo_input_pressed then
        boat_slomo_remaining_time = boat_slomo_refresh_duration
    end    
    boat_slomo_remaining_time = math.max(
        0.0,
        boat_slomo_remaining_time - math.max(0.0, routine_delta_time or 0.0)
    )
    physics_delta_time = routine_delta_time
    if boat_slomo_remaining_time > 0.0 then
        physics_delta_time = routine_delta_time * boat_slomo_time_scale
    end
        
    if is_input_locked() then
        update_forward_loop_sfx(false)
        boat_forward_speed = 0.0
        boat_turn_speed = 0.0
        return
    end

    local root, mesh, camera = get_components()
    if root == nil or mesh == nil or Pawn == nil then
        update_forward_loop_sfx(false)
        return
    end

    local safe_input_delta_time = clamp(input_delta_time or 0.0, 0.0001, 0.05)
    local safe_physics_delta_time = clamp(physics_delta_time or 0.0, 0.0, 0.05)
    local throttle_input = 0.0
    if is_forward_pressed then
        throttle_input = throttle_input + 1.0
    end
    if is_backward_pressed then
        throttle_input = throttle_input - 1.0
    end

    local steer_input = 0.0
    if is_turn_right_pressed then
        steer_input = steer_input + 1.0
    end
    if is_turn_left_pressed then
        steer_input = steer_input - 1.0
    end

    local cargo_weight = 0.0
    if GetDriftSalvageWeight then
        cargo_weight = math.max(0.0, GetDriftSalvageWeight())
    end

    local cargo_weight_capacity = 1.0
    if GetDriftSalvageWeightCapacity then
        cargo_weight_capacity = math.max(0.001, GetDriftSalvageWeightCapacity())
    end

    local cargo_weight_ratio = clamp(cargo_weight / cargo_weight_capacity, 0.0, 1.0)
    local mass = math.max(1.0, base_mass + cargo_weight_ratio * cargo_mass_scale)
    local mass_scale = 1.0 / math.max(1.0, mass or 1.0)
    local forward_accel_delta = boat_forward_accel * mass_scale * safe_physics_delta_time
    local reverse_accel_delta = boat_reverse_accel * mass_scale * safe_physics_delta_time
    local brake_accel_delta = boat_brake_accel * mass_scale * safe_physics_delta_time
    local linear_drag_delta = boat_linear_drag * mass_scale * safe_physics_delta_time
    local turn_damping_delta = boat_turn_damping * mass_scale * safe_input_delta_time

    if throttle_input > 0.0 then
        boat_forward_speed = boat_forward_speed + forward_accel_delta
    elseif throttle_input < 0.0 then
        if boat_forward_speed > 0.0 then
            boat_forward_speed = boat_forward_speed - brake_accel_delta
        else
            boat_forward_speed = boat_forward_speed - reverse_accel_delta
        end
    else
        boat_forward_speed = move_toward(
            boat_forward_speed,
            0.0,
            linear_drag_delta
        )
    end

    boat_forward_speed = clamp(
        boat_forward_speed,
        -boat_max_reverse_speed,
        boat_max_forward_speed
    )

    update_forward_loop_sfx(math.abs(boat_forward_speed) > boat_forward_loop_stop_speed)

    local speed_ratio = clamp(
        math.abs(boat_forward_speed) / math.max(boat_max_forward_speed, 0.001),
        0.0,
        1.0
    )
    local turn_ratio = speed_ratio * speed_ratio * (3.0 - 2.0 * speed_ratio)
    local turn_accel = boat_min_turn_accel +
        (boat_max_turn_accel - boat_min_turn_accel) * turn_ratio
    local turn_accel_delta = turn_accel * mass_scale * safe_input_delta_time
    if steer_input ~= 0.0 then
        boat_turn_speed = boat_turn_speed + steer_input * turn_accel_delta
    else
        boat_turn_speed = move_toward(
            boat_turn_speed,
            0.0,
            turn_damping_delta
        )
    end

    local max_turn_speed = math.max(
        boat_min_turn_accel,
        boat_max_turn_speed * turn_ratio
    )
    boat_turn_speed = clamp(
        boat_turn_speed,
        -max_turn_speed,
        max_turn_speed
    )

    if boat_turn_speed ~= 0.0 then
        mesh:Rotate(boat_turn_speed * safe_input_delta_time, 0.0)
    end

    local forward = Pawn:GetForwardVector()
    local move_scale = boat_forward_speed * safe_physics_delta_time
    Pawn:AddPosition(forward.X * move_scale, forward.Y * move_scale, 0.0)
end

function OnKeyDown(key)
    if key == "W" then
        is_forward_pressed = true
    elseif key == "S" then
        is_backward_pressed = true
    elseif key == "A" then
        is_turn_left_pressed = true
    elseif key == "D" then
        is_turn_right_pressed = true
    end
end

function OnKeyUp(key)
    if key == "W" then
        is_forward_pressed = false
    elseif key == "S" then
        is_backward_pressed = false
    elseif key == "A" then
        is_turn_left_pressed = false
    elseif key == "D" then
        is_turn_right_pressed = false
    elseif key == "P" then
        Camera.Shake({
            Type = "WaveOscillator",
            Duration = 3,
            LocationAmplitude = Vec3.new(1.0, 1.0, 1.0),
            LocationFrequency = Vec3.new(1.0, 1.0, 1.0),
            RotationAmplitude = Vec3.new(1.0, 1.0, 1.0),
            RotationFrequency = Vec3.new(1.0, 1.0, 1.0),
            FOVAmplitude = 30.0 * math.pi / 180.0,
            FOVFrequency = 1.0
        })
    elseif key == "L" then
        Camera.Shake({
            Type = "CameraSequence",
            Sequence = "SampleCameraShake",
            Scale = 0.1
        })
    end
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
    if button == "RightMouseButton" then
        if is_pressed then
            if not is_slomo_input_pressed then
                is_slomo_input_pressed = true
                start_routine(boat_slomo_visual_routine)
            end
        else
            is_slomo_input_pressed = false
            boat_slomo_remaining_time = 0.0
        end

        return
    end
end

function OnDestroy()
    update_forward_loop_sfx(false)
end
