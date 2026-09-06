-- LogoHud.lua

local ShowHud
local MakeHoverLabel

local HUD_BG_SHEET = "Asset/Texture/UI/BG.png"
local SCORE_BG_PATH = "Asset/Texture/UI/ScoreBoardBG.png"
local ENDING_SHEET = "Asset/Texture/UI/Ending.png"
local SCORE_MANAGER_PATH = "Asset/Scripts/ScoreManager.lua"

local MONEY_SHEET = "Asset/Texture/UI/Money.png"
local MONEY_ICON_SIZE = 96
local MONEY_FONT_SIZE = 56
local MONEY_TEXT_W = 180
local MONEY_GAP = 10
local SHIP_WHEEL_SHEET = "Asset/Texture/UI/ShipWheel.png"
local SHIP_WHEEL_SIZE = 480
local SHIP_WHEEL_SCREEN_X = 0.5
local SHIP_WHEEL_SCREEN_Y = 1.0
local SHIP_WHEEL_ROT_SPEED = 90.0
local SHIP_WHEEL_RELEASE_KICK = 16.0
local SHIP_WHEEL_RELEASE_DURATION = 0.45

local HEART_SHEET = "Asset/Texture/UI/Heart.png"
local HEART_COLS = 5
local HEART_COUNT = 5
local HEART_ANIM_FPS = 12.0
local HEART_FRAME_W = 17
local HEART_FRAME_H = 17
local HEART_DRAW_SCALE = 3.0
local HEART_DRAW_W = HEART_FRAME_W * HEART_DRAW_SCALE
local HEART_DRAW_H = HEART_FRAME_H * HEART_DRAW_SCALE
local HEART_SPACING = 8
local HEART_ROW_W = HEART_COUNT * HEART_DRAW_W + (HEART_COUNT - 1) * HEART_SPACING
local MONEY_ROW_W = MONEY_ICON_SIZE + MONEY_GAP + MONEY_TEXT_W
local PROGRESS_BG_SHEET = "Asset/Texture/UI/WeightPBBG.png"
local PROGRESS_FILL_SHEET = "Asset/Texture/UI/WeightPB.png"
local PROGRESS_ANCHOR_SHEET = "Asset/Texture/UI/Anchor.png"

-- Minimap (boat 중심 우상단)
local MINIMAP_SIZE = 260
local MINIMAP_RELATIVE_X = 0.86   -- 화면 중앙 기준 가로 비율 (panel 중심)
local MINIMAP_RELATIVE_Y = 0.20   -- 화면 중앙 기준 세로 비율 (panel 중심)
local MINIMAP_RANGE_HALF = 75.0   -- boat 중심에서 보여줄 world 반경 (XZ)
local MINIMAP_EDGE_PADDING = 8    -- 사거리 밖 아이콘을 패널 가장자리로부터 안쪽 여백
local MINIMAP_DOT_SIZE = 8
local MINIMAP_ICON_SIZE = 24
local MINIMAP_BG_PNG = "Asset/Mesh/MiniMap.png"
local MINIMAP_BOAT_PNG = "Asset/Texture/UI/Icon_boat.png"          -- placeholder
local MINIMAP_LIGHTHOUSE_PNG = "Asset/Texture/UI/Icon_lighthouse.png"       -- placeholder (Anchor 모양)
local MINIMAP_LIGHTHOUSE_ICON_SIZE = 30                            -- 등대는 살짝 크게
local MINIMAP_LIGHTHOUSE_TAGS = { "Lighthouse", "LightHouse" }
-- 태그별 색상 (R, G, B)
local MINIMAP_TAG_COLORS = {
    Trash      = { 0.7, 0.7, 0.7 },
    Resource   = { 0.3, 0.8, 1.0 },
    Recyclable = { 0.3, 0.9, 0.3 },
    Premium    = { 1.0, 0.85, 0.2 },
    Hazard     = { 1.0, 0.25, 0.25 },
    Rock       = { 0.55, 0.4, 0.25 },
}
-- minimap 에 점으로 표시할 태그 목록 (순서 = z-order)
local MINIMAP_DOT_TAGS = { "Rock", "Trash", "Resource", "Recyclable", "Premium", "Hazard" }

local MinimapPanel = nil
local MinimapBoatIcon = nil
local MinimapLighthouseIcon = nil
local MinimapDotPool = {}     -- 재사용 풀 (flat array)
local MinimapDotCount = 0     -- 이번 프레임에 사용 중인 dot 개수

local PROGRESS_W = MONEY_ROW_W * 2
local PROGRESS_H = 96
local PROGRESS_ANCHOR_SIZE = 84
local PROGRESS_ANCHOR_OVERLAP = 0
local PROGRESS_GAP_TOP = 10
local PROGRESS_GAP_BOTTOM = 8
local PROGRESS_ANIM_DURATION = 0.35
local DEFAULT_WEIGHT_CAPACITY = 150.0
local MONEY_Y = HEART_DRAW_H + PROGRESS_GAP_TOP
local PROGRESS_Y = MONEY_Y + MONEY_ICON_SIZE + PROGRESS_GAP_BOTTOM
local INGAME_HUD_W = math.max(HEART_ROW_W, PROGRESS_W, MONEY_ROW_W)
local INGAME_HUD_H = PROGRESS_Y + PROGRESS_H

local MenuPanel = nil
local ScorePanel = nil
local InGamePanel = nil
local GameOverPanel = nil
local ProgressBg = nil
local ProgressFill = nil
local ProgressAnchor = nil
local MoneyText = nil
local ProgressValue = 0.0
local ProgressStartValue = 0.0
local ProgressTargetValue = 0.0
local ProgressAnimTime = PROGRESS_ANIM_DURATION
local ShipWheel = nil
local ShipWheelAngle = 0.0
local ShipWheelLastDir = 0
local ShipWheelKickStartAngle = 0.0
local ShipWheelKickTargetAngle = 0.0
local ShipWheelKickTime = SHIP_WHEEL_RELEASE_DURATION
local HeartImages = {}
local bHeartAnimPlaying = false
local HeartAnimDir = 1
local HeartAnimIndex = 0
local HeartAnimTime = 0.0
local HeartAnimFrame = 0
local HeartHealth = HEART_COUNT
local HeartTargetHealth = HEART_COUNT
local BoatActor = nil
local BoatHomePosition = nil
local BoatHomeRotation = nil
local ScoreManager = nil

local function LoadScoreManager()
    if ScoreManager then
        return ScoreManager
    end

    local paths = {
        SCORE_MANAGER_PATH,
        "NipsEngine/" .. SCORE_MANAGER_PATH,
    }

    local lastError = nil
    for _, path in ipairs(paths) do
        local ok, managerOrError = pcall(dofile, path)
        if ok and managerOrError then
            ScoreManager = managerOrError
            return ScoreManager
        end
        lastError = managerOrError
    end

    if Log then
        Log("ScoreManager load failed: " .. tostring(lastError))
    end

    ScoreManager = {
        GetRecords = function()
            return { 0, 0, 0 }
        end,
        RecordScore = function(score)
            local records = { 0, math.max(0, math.floor(score or 0)), 0 }
            table.sort(records, function(a, b) return a > b end)
            return records
        end,
    }
    return ScoreManager
end

local function HeartUV(index)
    local col = math.max(0, math.min(HEART_COLS - 1, index or 0))
    return col / HEART_COLS, 0.0, (col + 1) / HEART_COLS, 1.0
end

local function Clamp01(value)
    return math.max(0.0, math.min(1.0, value or 0.0))
end

local function EnterUIMode()
    if SetUIMode then
        SetUIMode(true)
    end
end

local function EnterGameplayMode()
    if SetUIMode then
        SetUIMode(false)
    end
end

local function GetHudHealth()
    if GetDriftSalvageHealth then
        return GetDriftSalvageHealth()
    end

    return HEART_COUNT
end

local function GetHudMoney()
    if GetDriftSalvageMoney then
        return GetDriftSalvageMoney()
    end

    return 0
end

local function GetHudWeight()
    if GetDriftSalvageWeight then
        return GetDriftSalvageWeight()
    end

    return 0.0
end

local function GetHudWeightCapacity()
    if GetDriftSalvageWeightCapacity then
        return math.max(0.001, GetDriftSalvageWeightCapacity())
    end

    return DEFAULT_WEIGHT_CAPACITY
end

local function EaseSmoothStep(t)
    t = Clamp01(t)
    return t * t * (3.0 - 2.0 * t)
end

local function Lerp(a, b, t)
    return a + (b - a) * t
end

local function ResolveBoatActor()
    if BoatActor and BoatActor:IsValid() then
        return BoatActor
    end

    if FindActorByTag then
        BoatActor = FindActorByTag("Boat")
    end

    if BoatActor and BoatActor:IsValid() and not BoatHomePosition then
        BoatHomePosition = BoatActor:GetPosition()
        BoatHomeRotation = BoatActor:GetRotation()
    end

    return BoatActor
end

local function FindFirstActorByTags(tags)
    if not FindActorByTag then
        return nil
    end

    for _, tag in ipairs(tags) do
        local actor = FindActorByTag(tag)
        if actor and actor:IsValid() then
            return actor
        end
    end

    return nil
end

local function GetActorYawDegrees(actor)
    if not actor then
        return 0.0
    end

    if actor.GetForwardVector then
        local forward = actor:GetForwardVector()
        if forward and (math.abs(forward.X or 0.0) > 0.0001 or math.abs(forward.Y or 0.0) > 0.0001) then
            return math.deg(math.atan(forward.Y or 0.0, forward.X or 1.0))
        end
    end

    local rot = actor:GetRotation()
    if not rot then
        return 0.0
    end

    return rot.Z or rot.Y or 0.0
end

local function SetBoatAtHome()
    local boat = ResolveBoatActor()
    if not boat then return end

    if BoatHomePosition then
        boat:SetPosition(BoatHomePosition.X, BoatHomePosition.Y, BoatHomePosition.Z)
    end

    if BoatHomeRotation then
        boat:SetRotation(BoatHomeRotation.X, BoatHomeRotation.Y, BoatHomeRotation.Z)
    end
end

local function PrepareMenuPresentation()
    EnterUIMode()
    SetBoatAtHome()
end

local function GetHeartAnimFrame(time, direction)
    local duration = HEART_COLS / HEART_ANIM_FPS
    local ratio = EaseSmoothStep(time / duration)
    local frame = math.floor(ratio * (HEART_COLS - 1) + 0.5)

    if direction < 0 then
        return HEART_COLS - 1 - frame
    end

    return frame
end

local function ApplyHeartFrame(index, frame)
    local heart = HeartImages[index]
    if not heart then return end

    local u1, v1, u2, v2 = HeartUV(frame)
    heart:SetUV(u1, v1, u2, v2)
end

local function ApplyHeartHealth(health)
    local clamped = math.max(0, math.min(HEART_COUNT, math.floor((health or 0) + 0.5)))
    HeartHealth = clamped
    HeartTargetHealth = clamped
    bHeartAnimPlaying = false
    HeartAnimIndex = 0

    for i = 1, HEART_COUNT do
        ApplyHeartFrame(i, i <= clamped and 0 or HEART_COLS - 1)
    end
end

local function StartHeartAnimStep()
    if HeartTargetHealth == HeartHealth then
        return
    end

    HeartAnimDir = HeartTargetHealth < HeartHealth and 1 or -1
    HeartAnimIndex = HeartAnimDir > 0 and HeartHealth or HeartHealth + 1
    HeartAnimTime = 0.0
    HeartAnimFrame = HeartAnimDir > 0 and 0 or HEART_COLS - 1
    bHeartAnimPlaying = true
    ApplyHeartFrame(HeartAnimIndex, HeartAnimFrame)
end

local function SetHeartTargetHealth(health)
    HeartTargetHealth = math.max(0, math.min(HEART_COUNT, math.floor((health or 0) + 0.5)))

    if not bHeartAnimPlaying and HeartTargetHealth ~= HeartHealth then
        StartHeartAnimStep()
    end
end

local function SetProgressTarget(value)
    ProgressStartValue = ProgressValue
    ProgressTargetValue = Clamp01(value)
    ProgressAnimTime = 0.0
end

local function ApplyProgressValue(value)
    ProgressValue = Clamp01(value)

    if ProgressFill then
        ProgressFill:SetSize(PROGRESS_W * ProgressValue, PROGRESS_H)
        ProgressFill:SetUV(0.0, 0.0, ProgressValue, 1.0)
        ProgressFill:SetVisible(ProgressValue > 0.0)
    end
end

local function UpdateProgressBar(deltaTime)
    if not ProgressFill or ProgressAnimTime >= PROGRESS_ANIM_DURATION then return end

    ProgressAnimTime = math.min(PROGRESS_ANIM_DURATION, ProgressAnimTime + (deltaTime or 0.0))
    local ratio = EaseSmoothStep(ProgressAnimTime / PROGRESS_ANIM_DURATION)

    ApplyProgressValue(ProgressStartValue + (ProgressTargetValue - ProgressStartValue) * ratio)
end

local function SyncGameplayHud()
    if not InGamePanel then return end

    if MoneyText then
        MoneyText:SetText(tostring(GetHudMoney()) .. "$")
    end

    local targetWeightRatio = Clamp01(GetHudWeight() / GetHudWeightCapacity())
    if math.abs(targetWeightRatio - ProgressTargetValue) > 0.001 then
        SetProgressTarget(targetWeightRatio)
    end

    local health = GetHudHealth()
    if health ~= HeartTargetHealth then
        SetHeartTargetHealth(health)
    end
end

local function UpdateShipWheel(deltaTime)
    if not ShipWheel or not Input then return end

    local dt = deltaTime or 0.0
    local dir = 0
    if Input.GetKey("Left") or Input.GetKey("A") then
        dir = dir - 1
    end
    if Input.GetKey("Right") or Input.GetKey("D") then
        dir = dir + 1
    end

    if dir ~= 0 then
        ShipWheelAngle = (ShipWheelAngle + dir * SHIP_WHEEL_ROT_SPEED * dt) % 360.0
        ShipWheelLastDir = dir
        ShipWheelKickTime = SHIP_WHEEL_RELEASE_DURATION
    elseif ShipWheelLastDir ~= 0 and ShipWheelKickTime >= SHIP_WHEEL_RELEASE_DURATION then
        ShipWheelKickStartAngle = ShipWheelAngle
        ShipWheelKickTargetAngle = ShipWheelAngle - ShipWheelLastDir * SHIP_WHEEL_RELEASE_KICK
        ShipWheelKickTime = 0.0
        ShipWheelLastDir = 0
    elseif ShipWheelKickTime < SHIP_WHEEL_RELEASE_DURATION then
        ShipWheelKickTime = math.min(SHIP_WHEEL_RELEASE_DURATION, ShipWheelKickTime + dt)
        local ratio = EaseSmoothStep(ShipWheelKickTime / SHIP_WHEEL_RELEASE_DURATION)
        ShipWheelAngle = (ShipWheelKickStartAngle + (ShipWheelKickTargetAngle - ShipWheelKickStartAngle) * ratio) % 360.0
    end

    ShipWheel:SetRotation(ShipWheelAngle)
end

local function AcquireMinimapDot(r, g, b)
    MinimapDotCount = MinimapDotCount + 1
    local dot = MinimapDotPool[MinimapDotCount]
    if not dot then
        dot = UIManager.CreateImage(MinimapPanel, 0, 0, MINIMAP_DOT_SIZE, MINIMAP_DOT_SIZE, nil, "Centered")
        MinimapDotPool[MinimapDotCount] = dot
    end
    dot:SetVisible(true)
    dot:SetColor(r, g, b, 1.0)
    return dot
end

local function HideUnusedMinimapDots()
    for i = MinimapDotCount + 1, #MinimapDotPool do
        local dot = MinimapDotPool[i]
        if dot then dot:SetVisible(false) end
    end
end

local function WorldDeltaToBoatLocal(boatPos, targetPos, boatYawDegrees)
    local dx = targetPos.X - boatPos.X
    local dy = targetPos.Y - boatPos.Y
    local yaw = math.rad(boatYawDegrees or 0.0)
    local cosYaw = math.cos(yaw)
    local sinYaw = math.sin(yaw)

    local localForward = dx * cosYaw + dy * sinYaw
    local localRight = -dx * sinYaw + dy * cosYaw
    return localForward, localRight
end

local function ProjectToMinimap(boatPos, targetPos, boatYawDegrees)
    -- world XY 차이를 boat 기준 앞/오른쪽 좌표로 바꾼 뒤 minimap pixel 로 변환
    local localForward, localRight = WorldDeltaToBoatLocal(boatPos, targetPos, boatYawDegrees)
    local half = MINIMAP_SIZE * 0.5
    local px = (localRight / MINIMAP_RANGE_HALF) * half
    local py = -(localForward / MINIMAP_RANGE_HALF) * half
    -- 사거리 밖이면 nil 리턴 (dot/icon hide)
    if math.abs(px) > half or math.abs(py) > half then
        return nil, nil
    end
    return px, py
end

-- 사거리 밖이면 panel 가장자리로 clamp (off-range direction indicator 용)
local function ProjectToMinimapClamped(boatPos, targetPos, boatYawDegrees, iconSize)
    local localForward, localRight = WorldDeltaToBoatLocal(boatPos, targetPos, boatYawDegrees)
    local half = MINIMAP_SIZE * 0.5
    local px = (localRight / MINIMAP_RANGE_HALF) * half
    local py = -(localForward / MINIMAP_RANGE_HALF) * half
    local iconHalf = (iconSize or 0) * 0.5
    local limit = half - MINIMAP_EDGE_PADDING - iconHalf
    local maxAxis = math.max(math.abs(px), math.abs(py))
    local outOfRange = false
    if maxAxis > limit then
        local scale = limit / maxAxis
        px = px * scale
        py = py * scale
        outOfRange = true
    end
    return px, py, outOfRange
end

local function UpdateMinimap()
    if not MinimapPanel then return end

    local boat = ResolveBoatActor()
    if not boat then
        MinimapDotCount = 0
        HideUnusedMinimapDots()
        if MinimapBoatIcon then MinimapBoatIcon:SetVisible(false) end
        if MinimapLighthouseIcon then MinimapLighthouseIcon:SetVisible(false) end
        return
    end

    local boatPos = boat:GetPosition()
    local boatYaw = GetActorYawDegrees(boat)

    -- Boat 아이콘: 중심 고정. 주변 대상 좌표가 boat 기준으로 회전되므로 아이콘 자체는 기준점 역할만 한다.
    if MinimapBoatIcon then
        MinimapBoatIcon:SetVisible(true)
        MinimapBoatIcon:SetPosition(0, 0)
        MinimapBoatIcon:SetRotation(0.0)
    end

    -- Lighthouse 아이콘: 사거리 밖이면 panel 가장자리에 clamp 해 방향 표시
    local lighthouse = FindFirstActorByTags(MINIMAP_LIGHTHOUSE_TAGS)
    if lighthouse then
        local lpos = lighthouse:GetPosition()
        local px, py, outOfRange = ProjectToMinimapClamped(boatPos, lpos, boatYaw, MINIMAP_LIGHTHOUSE_ICON_SIZE)
        if MinimapLighthouseIcon then
            MinimapLighthouseIcon:SetVisible(true)
            MinimapLighthouseIcon:SetPosition(px, py)
            -- 사거리 밖일 때는 alpha 낮춰 "방향만" 임을 시각적으로 구분
            if outOfRange then
                MinimapLighthouseIcon:SetColor(1.0, 0.9, 0.2, 0.7)
            else
                MinimapLighthouseIcon:SetColor(1.0, 0.9, 0.2, 1.0)
            end
        end
    elseif MinimapLighthouseIcon then
        MinimapLighthouseIcon:SetVisible(false)
    end

    -- 일반 dot (태그별)
    MinimapDotCount = 0
    if FindActorsByTag then
        for _, tag in ipairs(MINIMAP_DOT_TAGS) do
            local color = MINIMAP_TAG_COLORS[tag]
            if color then
                local actors = FindActorsByTag(tag)
                if actors then
                    for _, actor in ipairs(actors) do
                        if actor and actor:IsValid() then
                            local pos = actor:GetPosition()
                            local px, py = ProjectToMinimap(boatPos, pos, boatYaw)
                            if px then
                                local dot = AcquireMinimapDot(color[1], color[2], color[3])
                                dot:SetPosition(px, py)
                            end
                        end
                    end
                end
            end
        end
    end
    HideUnusedMinimapDots()
end

local function HideMinimap()
    -- panel 을 destroy 하면 자식(dot/icon)도 함께 정리됨
    if MinimapPanel then
        UIManager.DestroyElement(MinimapPanel)
    end
    MinimapPanel = nil
    MinimapBoatIcon = nil
    MinimapLighthouseIcon = nil
    MinimapDotPool = {}
    MinimapDotCount = 0
end

local function ShowMinimap()
    if MinimapPanel then return end

    MinimapPanel = UIManager.CreateImage(nil,
        MINIMAP_RELATIVE_X, MINIMAP_RELATIVE_Y,
        MINIMAP_SIZE, MINIMAP_SIZE,
        MINIMAP_BG_PNG, "RelativePos")
    MinimapPanel:SetColor(1.0, 1.0, 1.0, 1.0)

    MinimapLighthouseIcon = UIManager.CreateImage(MinimapPanel,
        0, 0, MINIMAP_LIGHTHOUSE_ICON_SIZE, MINIMAP_LIGHTHOUSE_ICON_SIZE,
        MINIMAP_LIGHTHOUSE_PNG, "Centered")
    MinimapLighthouseIcon:SetColor(1.0, 0.9, 0.2, 1.0)   -- 노란빛 tint 로 식별력
    MinimapLighthouseIcon:SetVisible(false)

    -- Boat 는 항상 중심에 그려지므로 마지막에 추가 (위에 그려짐)
    MinimapBoatIcon = UIManager.CreateImage(MinimapPanel,
        0, 0, MINIMAP_ICON_SIZE, MINIMAP_ICON_SIZE,
        MINIMAP_BOAT_PNG, "Centered")
    MinimapBoatIcon:SetColor(1.0, 1.0, 1.0, 1.0)
end

local function HideInGameHud()
    EnterUIMode()

    if InGamePanel then
        UIManager.DestroyElement(InGamePanel)
        InGamePanel = nil
        ProgressBg = nil
        ProgressFill = nil
        ProgressAnchor = nil
        MoneyText = nil
        ProgressValue = 0.0
        ProgressStartValue = 0.0
        ProgressTargetValue = 0.0
        ProgressAnimTime = PROGRESS_ANIM_DURATION
        HeartImages = {}
        bHeartAnimPlaying = false
        HeartAnimDir = 1
        HeartAnimIndex = 0
        HeartAnimTime = 0.0
        HeartAnimFrame = 0
        HeartHealth = HEART_COUNT
        HeartTargetHealth = HEART_COUNT
    end

    if ShipWheel then
        UIManager.DestroyElement(ShipWheel)
        ShipWheel = nil
        ShipWheelAngle = 0.0
        ShipWheelLastDir = 0
        ShipWheelKickTime = SHIP_WHEEL_RELEASE_DURATION
    end

    HideMinimap()
end

local function ShowInGameHud()
    if InGamePanel then return end

    ProgressValue = Clamp01(GetHudWeight() / GetHudWeightCapacity())
    ProgressStartValue = ProgressValue
    ProgressTargetValue = ProgressValue
    ProgressAnimTime = PROGRESS_ANIM_DURATION

    InGamePanel = UIManager.CreateImage(nil, 20, 20, INGAME_HUD_W, INGAME_HUD_H, nil, nil)
    InGamePanel:SetColor(0.0, 0.0, 0.0, 0.0)

    HeartImages = {}
    local u1, v1, u2, v2 = HeartUV(0)

    for i = 1, HEART_COUNT do
        local x = (i - 1) * (HEART_DRAW_W + HEART_SPACING)
        local heart = UIManager.CreateImage(InGamePanel, x, 0, HEART_DRAW_W, HEART_DRAW_H, HEART_SHEET, nil)
        heart:SetUV(u1, v1, u2, v2)
        heart:SetColor(1.6, 1.6, 1.6, 1.0)
        heart:SetVisible(true)
        HeartImages[i] = heart
    end

    ProgressBg = UIManager.CreateImage(InGamePanel, 0, PROGRESS_Y, PROGRESS_W, PROGRESS_H, PROGRESS_BG_SHEET, nil)
    ProgressBg:SetColor(1.0, 1.0, 1.0, 1.0)

    ProgressFill = UIManager.CreateImage(InGamePanel, 0, PROGRESS_Y, PROGRESS_W, PROGRESS_H, PROGRESS_FILL_SHEET, nil)
    ProgressFill:SetColor(1.0, 1.0, 1.0, 1.0)
    ApplyProgressValue(ProgressValue)

    local anchorX = -PROGRESS_ANCHOR_OVERLAP
    local anchorY = PROGRESS_Y + (PROGRESS_H - PROGRESS_ANCHOR_SIZE) * 0.5
    ProgressAnchor = UIManager.CreateImage(InGamePanel, anchorX, anchorY, PROGRESS_ANCHOR_SIZE, PROGRESS_ANCHOR_SIZE, PROGRESS_ANCHOR_SHEET, nil)
    ProgressAnchor:SetColor(1.0, 1.0, 1.0, 1.0)

    local moneyIcon = UIManager.CreateImage(InGamePanel, 0, MONEY_Y, MONEY_ICON_SIZE, MONEY_ICON_SIZE, MONEY_SHEET, nil)
    moneyIcon:SetColor(1.25, 1.25, 1.25, 1.0)

    local moneyTextY = MONEY_Y + (MONEY_ICON_SIZE - MONEY_FONT_SIZE) * 0.5
    MoneyText = UIManager.CreateText(InGamePanel, MONEY_ICON_SIZE + MONEY_GAP, moneyTextY, MONEY_TEXT_W, MONEY_FONT_SIZE, tostring(GetHudMoney()) .. "$", MONEY_FONT_SIZE, nil)
    MoneyText:SetColor(1.0, 0.95, 0.25, 1.0)

    ShipWheel = UIManager.CreateImage(nil, SHIP_WHEEL_SCREEN_X, SHIP_WHEEL_SCREEN_Y, SHIP_WHEEL_SIZE, SHIP_WHEEL_SIZE, SHIP_WHEEL_SHEET, "RelativePos")
    ShipWheel:SetColor(1.0, 1.0, 1.0, 1.0)
    ShipWheel:SetRotation(ShipWheelAngle)

    bHeartAnimPlaying = false
    HeartAnimDir = 1
    HeartAnimIndex = 0
    HeartAnimTime = 0.0
    HeartAnimFrame = 0
    HeartTargetHealth = GetHudHealth()
    ApplyHeartHealth(GetHudHealth())
    SyncGameplayHud()
    
    ShowMinimap()
    UpdateMinimap()
end

local function HideScoreboard()
    if ScorePanel then
        UIManager.DestroyElement(ScorePanel)
        ScorePanel = nil
    end
end

local function HideGameOver()
    if GameOverPanel then
        UIManager.DestroyElement(GameOverPanel)
        GameOverPanel = nil
    end
end

local function CreateCenteredText(parent, x, y, text, fontSize)
    local value = tostring(text or "")
    local size = fontSize or 16.0
    return UIManager.CreateText(parent, x, y, math.max(size, #value * size), size, value, size, "Centered")
end

local function ShowScoreboard(records)
    if ScorePanel then return end

    ScorePanel = UIManager.CreateImage(nil, 0.5, 0.5, 448, 558, SCORE_BG_PATH, "RelativePos")
    ScorePanel:SetColor(1.0, 1.0, 1.0, 1.0)

    local title = CreateCenteredText(ScorePanel, 0, -186, "RECORD", 36.0)
    title:SetColor(0.0, 0.0, 0.0, 1.0)

    if type(records) ~= "table" then
        records = LoadScoreManager().GetRecords()
    end

    local rowY = { -100, -5, 90 }
    for rank = 1, 3 do
        local scoreText = CreateCenteredText(ScorePanel, 20, rowY[rank], records[rank] or 0, 60.0)
        scoreText:SetColor(0.0, 0.0, 0.0, 1.0)
    end

    local closeBtn = CreateCenteredText(ScorePanel, 0, 226, "CLOSE", 30.0)
    closeBtn:SetColor(0, 0, 0, 1.0)
    closeBtn:SetInteractable(true)
    closeBtn:OnHoverEnter(function() closeBtn:SetColor(1.0, 1.0, 0.0, 1.0) end)
    closeBtn:OnHoverExit(function() closeBtn:SetColor(0.0, 0.0, 0.0, 1.0) end)
    closeBtn:OnClick(function()
        HideScoreboard()
        ShowHud()
    end)
end

local function ShowGameOver()
    if GameOverPanel then return end

    EnterUIMode()
    local finalScore = GetHudMoney()
    LoadScoreManager().RecordScore(finalScore)
    HideInGameHud()

    GameOverPanel = UIManager.CreateImage(nil, 0.5, 0.5, 0.55, 0.55, nil, "FullRelative")
    GameOverPanel:SetColor(0.0, 0.0, 0.0, 0.0)

    local gameOverBg = UIManager.CreateImage(GameOverPanel, 0.0, 0.0, 0.75, 1.5, HUD_BG_SHEET, "ParentRelative")
    gameOverBg:SetColor(1.0, 1.0, 1.0, 1.0)

    local endingImage = UIManager.CreateImage(GameOverPanel, 0.0, -0.34, 0.46, 0.42, ENDING_SHEET, "ParentRelative")
    endingImage:SetColor(1.0, 1.0, 1.0, 1.0)

    local scoreText = UIManager.CreateText(GameOverPanel, -0.03, 0.0, 400, 72, "SCORE : " .. tostring(finalScore), 48.0, "RelativePos")
    scoreText:SetColor(0.0, 0.0, 0.0, 1.0)

    local restartLabel = UIManager.CreateText(GameOverPanel, -0.01, 0.15, 280, 48, "RESTART", 48.0, "RelativePos")
    restartLabel:SetColor(0.0, 0.0, 0.0, 1.0)
    restartLabel:SetInteractable(true)
    restartLabel:OnHoverEnter(function() restartLabel:SetColor(1.0, 1.0, 0.0, 1.0) end)
    restartLabel:OnHoverExit(function() restartLabel:SetColor(0.0, 0.0, 0.0, 1.0) end)
    restartLabel:OnClick(function()
        _G.LogoHudNextStartMode = "Gameplay"
        RequestGameRestart()
    end)

    local titleLabel = UIManager.CreateText(GameOverPanel, 0.01, 0.27, 280, 48, "TITLE", 48.0, "RelativePos")
    titleLabel:SetColor(0.0, 0.0, 0.0, 1.0)
    titleLabel:SetInteractable(true)
    titleLabel:OnHoverEnter(function() titleLabel:SetColor(1.0, 1.0, 0.0, 1.0) end)
    titleLabel:OnHoverExit(function() titleLabel:SetColor(0.0, 0.0, 0.0, 1.0) end)
    titleLabel:OnClick(function()
        _G.LogoHudNextStartMode = "Title"
        RequestGameRestart()
    end)
end

MakeHoverLabel = function(parent, x, y, w, h, text, fontSize, mode)
    local label = UIManager.CreateText(parent, x, y, w, h, text, fontSize, mode)
    label:SetInteractable(true)
    label:OnHoverEnter(function() label:SetColor(1.0, 1.0, 0.0, 1.0) end)
    label:OnHoverExit(function() label:SetColor(0.0, 0.0, 0.0, 1.0) end)
    return label
end

local function HideHud()
    if MenuPanel then
        UIManager.DestroyElement(MenuPanel)
        MenuPanel = nil
    end
end

local function StartGameplay()
    HideHud()
    ShowInGameHud()
    if ResetDriftSalvageStats then
        ResetDriftSalvageStats()
    end
    SetBoatAtHome()
    EnterGameplayMode()
end

ShowHud = function()
    if MenuPanel then return end

    HideInGameHud()
    HideGameOver()

    MenuPanel = UIManager.CreateImage(nil, 0.5, 0.5, 0.4, 0.5, nil, "FullRelative")
    MenuPanel:SetColor(0.0, 0.0, 0.0, 0.0)

    local menuBg = UIManager.CreateImage(MenuPanel, 0.0, 0.0, 1.0, 1.5, HUD_BG_SHEET, "ParentRelative")
    menuBg:SetColor(1.0, 1.0, 1.0, 1.0)

    UIManager.CreateImage(MenuPanel, 0.0, -0.3, 0.8, 0.3, "Asset/Texture/UI/Logo.png", "ParentRelative")
    UIManager.CreateImage(MenuPanel, -0.25, 0.1, 0.15, 0.25, "Asset/Texture/UI/Icon_boat.png", "ParentRelative")

    local StartLabel = MakeHoverLabel(MenuPanel, 0.025, 0.03, 256, 28, "START", 48.0, "RelativePos")
    StartLabel:OnClick(function()
        StartGameplay()
    end)

    UIManager.CreateImage(MenuPanel, -0.25, 0.4, 0.15, 0.25, "Asset/Texture/UI/Icon_book.png", "ParentRelative")

    local RecordLabel = MakeHoverLabel(MenuPanel, 0.025, 0.18, 256, 28, "RECORD", 48.0, "RelativePos")
    RecordLabel:OnClick(function()
        HideHud()
        ShowScoreboard(LoadScoreManager().GetRecords())
    end)

    local ok, err = pcall(PrepareMenuPresentation)
    if not ok and Log then
        Log("PrepareMenuPresentation failed: " .. tostring(err))
    end
end

function OnStart(self)
    local nextStartMode = _G.LogoHudNextStartMode
    _G.LogoHudNextStartMode = nil

    if nextStartMode == "Gameplay" then
        StartGameplay()
        return
    end

    ShowHud()
end

function OnUpdate(self, deltaTime)
    -- 충돌 코드는 하트가 끝나기 전에도 즉시 Game Over를 요청 (Boat-LightHouse 용)
    if InGamePanel and ConsumeDriftSalvageGameOverRequest and ConsumeDriftSalvageGameOverRequest() then
        ShowGameOver()
        return
    end

    if InGamePanel and Input and Input.GetKeyDown("E") then
        ShowGameOver()
        return
    end

    SyncGameplayHud()
    UpdateProgressBar(deltaTime)
    UpdateShipWheel(deltaTime)
    UpdateMinimap()

    if bHeartAnimPlaying then
        HeartAnimTime = HeartAnimTime + (deltaTime or 0.0)
        local duration = HEART_COLS / HEART_ANIM_FPS
        local nextFrame = GetHeartAnimFrame(HeartAnimTime, HeartAnimDir)

        if HeartAnimTime >= duration then
            bHeartAnimPlaying = false

            if HeartAnimDir > 0 then
                HeartAnimFrame = HEART_COLS - 1
                HeartHealth = math.max(0, HeartHealth - 1)
            else
                HeartAnimFrame = 0
                HeartHealth = math.min(HEART_COUNT, HeartHealth + 1)
            end

            ApplyHeartFrame(HeartAnimIndex, HeartAnimFrame)
            HeartAnimIndex = 0
            StartHeartAnimStep()
        elseif nextFrame ~= HeartAnimFrame then
            HeartAnimFrame = nextFrame
            ApplyHeartFrame(HeartAnimIndex, HeartAnimFrame)
        end
    end

    if InGamePanel and GetHudHealth() <= 0 and not bHeartAnimPlaying then
        ShowGameOver()
    end
end

function OnDestroy(self)
    EnterUIMode()
    HideHud()
    HideScoreboard()
    HideGameOver()
    HideInGameHud()
end
