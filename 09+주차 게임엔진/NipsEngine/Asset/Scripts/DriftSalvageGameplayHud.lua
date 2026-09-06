-- DriftSalvageGameplayHud.lua

local GameplayHud = {}

local TIMER_LABEL_TEXT = "TIME"
local TIMER_LABEL_W = 114
local TIMER_LABEL_FONT_SIZE = 42
local TIMER_LABEL_LETTER_GAP = 14
local TIMER_DIGIT_W = 28
local TIMER_DIGIT_H = 44
local TIMER_SEPARATOR_W = 14
local TIMER_DIGIT_GAP = 2
local TIMER_GAP = 42
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

local PROGRESS_BG_SHEET = "Asset/Texture/UI/WeightPBBG.png"
local PROGRESS_FILL_SHEET = "Asset/Texture/UI/WeightPB.png"
local PROGRESS_ANCHOR_SHEET = "Asset/Texture/UI/Anchor.png"
local TIMER_ROW_W = TIMER_LABEL_W + TIMER_GAP + TIMER_DIGIT_W * 6 + TIMER_SEPARATOR_W * 2 + TIMER_DIGIT_GAP * 7
local PROGRESS_W = 572
local PROGRESS_H = 96
local PROGRESS_ANCHOR_SIZE = 84
local PROGRESS_ANCHOR_OVERLAP = 0
local PROGRESS_GAP_TOP = 10
local PROGRESS_GAP_BOTTOM = 8
local PROGRESS_ANIM_DURATION = 0.35
local TIMER_Y = HEART_DRAW_H + PROGRESS_GAP_TOP
local TIMER_ROW_H = math.max(TIMER_LABEL_FONT_SIZE, TIMER_DIGIT_H)
local PROGRESS_Y = TIMER_Y + TIMER_ROW_H + PROGRESS_GAP_BOTTOM
local INGAME_HUD_W = math.max(HEART_ROW_W, PROGRESS_W, TIMER_ROW_W)
local INGAME_HUD_H = PROGRESS_Y + PROGRESS_H

local function HeartUV(index)
    local col = math.max(0, math.min(HEART_COLS - 1, index or 0))
    return col / HEART_COLS, 0.0, (col + 1) / HEART_COLS, 1.0
end

function GameplayHud.Create(deps)
    local state = {
        Panel = nil,
        ProgressBg = nil,
        ProgressFill = nil,
        ProgressAnchor = nil,
        TimerLabel = nil,
        TimerDisplay = nil,
        ProgressValue = 0.0,
        ProgressStartValue = 0.0,
        ProgressTargetValue = 0.0,
        ProgressAnimTime = PROGRESS_ANIM_DURATION,
        ShipWheel = nil,
        ShipWheelAngle = 0.0,
        ShipWheelLastDir = 0,
        ShipWheelKickStartAngle = 0.0,
        ShipWheelKickTargetAngle = 0.0,
        ShipWheelKickTime = SHIP_WHEEL_RELEASE_DURATION,
        HeartImages = {},
        bHeartAnimPlaying = false,
        HeartAnimDir = 1,
        HeartAnimIndex = 0,
        HeartAnimTime = 0.0,
        HeartAnimFrame = 0,
        HeartHealth = HEART_COUNT,
        HeartTargetHealth = HEART_COUNT,
        Minimap = deps.Minimap,
        TimeDisplay = deps.TimeDisplay,
        Clamp01 = deps.Clamp01,
        EaseSmoothStep = deps.EaseSmoothStep,
        EnterUIMode = deps.EnterUIMode,
        GetElapsedTime = deps.GetElapsedTime,
        GetWeight = deps.GetWeight,
        GetWeightCapacity = deps.GetWeightCapacity,
        GetHealth = deps.GetHealth,
    }

    local function ApplyHeartFrame(index, frame)
        local heart = state.HeartImages[index]
        if not heart then return end

        local u1, v1, u2, v2 = HeartUV(frame)
        heart:SetUV(u1, v1, u2, v2)
    end

    local function GetHeartAnimFrame(time, direction)
        local duration = HEART_COLS / HEART_ANIM_FPS
        local ratio = state.EaseSmoothStep(time / duration)
        local frame = math.floor(ratio * (HEART_COLS - 1) + 0.5)

        if direction < 0 then
            return HEART_COLS - 1 - frame
        end

        return frame
    end

    local function StartHeartAnimStep()
        if state.HeartTargetHealth == state.HeartHealth then
            return
        end

        state.HeartAnimDir = state.HeartTargetHealth < state.HeartHealth and 1 or -1
        state.HeartAnimIndex = state.HeartAnimDir > 0 and state.HeartHealth or state.HeartHealth + 1
        state.HeartAnimTime = 0.0
        state.HeartAnimFrame = state.HeartAnimDir > 0 and 0 or HEART_COLS - 1
        state.bHeartAnimPlaying = true
        ApplyHeartFrame(state.HeartAnimIndex, state.HeartAnimFrame)
    end

    local function ApplyHeartHealth(health)
        local clamped = math.max(0, math.min(HEART_COUNT, math.floor((health or 0) + 0.5)))
        state.HeartHealth = clamped
        state.HeartTargetHealth = clamped
        state.bHeartAnimPlaying = false
        state.HeartAnimIndex = 0

        for i = 1, HEART_COUNT do
            ApplyHeartFrame(i, i <= clamped and 0 or HEART_COLS - 1)
        end
    end

    local function SetHeartTargetHealth(health)
        state.HeartTargetHealth = math.max(0, math.min(HEART_COUNT, math.floor((health or 0) + 0.5)))

        if not state.bHeartAnimPlaying and state.HeartTargetHealth ~= state.HeartHealth then
            StartHeartAnimStep()
        end
    end

    local function ApplyProgressValue(value)
        state.ProgressValue = state.Clamp01(value)

        if state.ProgressFill then
            state.ProgressFill:SetSize(PROGRESS_W * state.ProgressValue, PROGRESS_H)
            state.ProgressFill:SetUV(0.0, 0.0, state.ProgressValue, 1.0)
            state.ProgressFill:SetVisible(state.ProgressValue > 0.0)
        end
    end

    local function SetProgressTarget(value)
        state.ProgressStartValue = state.ProgressValue
        state.ProgressTargetValue = state.Clamp01(value)
        state.ProgressAnimTime = 0.0
    end

    local function UpdateProgressBar(deltaTime)
        if not state.ProgressFill or state.ProgressAnimTime >= PROGRESS_ANIM_DURATION then return end

        state.ProgressAnimTime = math.min(PROGRESS_ANIM_DURATION, state.ProgressAnimTime + (deltaTime or 0.0))
        local ratio = state.EaseSmoothStep(state.ProgressAnimTime / PROGRESS_ANIM_DURATION)

        ApplyProgressValue(state.ProgressStartValue + (state.ProgressTargetValue - state.ProgressStartValue) * ratio)
    end

    local function UpdateShipWheel(deltaTime)
        if not state.ShipWheel or not Input then return end

        local dt = deltaTime or 0.0
        local dir = 0
        if Input.GetKey("Left") or Input.GetKey("A") then
            dir = dir - 1
        end
        if Input.GetKey("Right") or Input.GetKey("D") then
            dir = dir + 1
        end

        if dir ~= 0 then
            state.ShipWheelAngle = (state.ShipWheelAngle + dir * SHIP_WHEEL_ROT_SPEED * dt) % 360.0
            state.ShipWheelLastDir = dir
            state.ShipWheelKickTime = SHIP_WHEEL_RELEASE_DURATION
        elseif state.ShipWheelLastDir ~= 0 and state.ShipWheelKickTime >= SHIP_WHEEL_RELEASE_DURATION then
            state.ShipWheelKickStartAngle = state.ShipWheelAngle
            state.ShipWheelKickTargetAngle = state.ShipWheelAngle - state.ShipWheelLastDir * SHIP_WHEEL_RELEASE_KICK
            state.ShipWheelKickTime = 0.0
            state.ShipWheelLastDir = 0
        elseif state.ShipWheelKickTime < SHIP_WHEEL_RELEASE_DURATION then
            state.ShipWheelKickTime = math.min(SHIP_WHEEL_RELEASE_DURATION, state.ShipWheelKickTime + dt)
            local ratio = state.EaseSmoothStep(state.ShipWheelKickTime / SHIP_WHEEL_RELEASE_DURATION)
            state.ShipWheelAngle = (state.ShipWheelKickStartAngle + (state.ShipWheelKickTargetAngle - state.ShipWheelKickStartAngle) * ratio) % 360.0
        end

        state.ShipWheel:SetRotation(state.ShipWheelAngle)
    end

    local function UpdateHeartAnimation(deltaTime)
        if not state.bHeartAnimPlaying then return end

        state.HeartAnimTime = state.HeartAnimTime + (deltaTime or 0.0)
        local duration = HEART_COLS / HEART_ANIM_FPS
        local nextFrame = GetHeartAnimFrame(state.HeartAnimTime, state.HeartAnimDir)

        if state.HeartAnimTime >= duration then
            state.bHeartAnimPlaying = false

            if state.HeartAnimDir > 0 then
                state.HeartAnimFrame = HEART_COLS - 1
                state.HeartHealth = math.max(0, state.HeartHealth - 1)
            else
                state.HeartAnimFrame = 0
                state.HeartHealth = math.min(HEART_COUNT, state.HeartHealth + 1)
            end

            ApplyHeartFrame(state.HeartAnimIndex, state.HeartAnimFrame)
            state.HeartAnimIndex = 0
            StartHeartAnimStep()
        elseif nextFrame ~= state.HeartAnimFrame then
            state.HeartAnimFrame = nextFrame
            ApplyHeartFrame(state.HeartAnimIndex, state.HeartAnimFrame)
        end
    end

    function state:IsVisible()
        return self.Panel ~= nil
    end

    function state:IsHeartAnimating()
        return self.bHeartAnimPlaying
    end

    function state:Sync()
        if not self.Panel then return end

        if self.TimerDisplay then
            self.TimerDisplay:SetValue(self.GetElapsedTime())
        end

        local targetWeightRatio = self.Clamp01(self.GetWeight() / self.GetWeightCapacity())
        if math.abs(targetWeightRatio - self.ProgressTargetValue) > 0.001 then
            SetProgressTarget(targetWeightRatio)
        end

        local health = self.GetHealth()
        if health ~= self.HeartTargetHealth then
            SetHeartTargetHealth(health)
        end
    end

    function state:Show()
        if self.Panel then return end

        self.ProgressValue = self.Clamp01(self.GetWeight() / self.GetWeightCapacity())
        self.ProgressStartValue = self.ProgressValue
        self.ProgressTargetValue = self.ProgressValue
        self.ProgressAnimTime = PROGRESS_ANIM_DURATION

        self.Panel = UIManager.CreateImage(nil, 20, 20, INGAME_HUD_W, INGAME_HUD_H, nil, nil)
        self.Panel:SetColor(0.0, 0.0, 0.0, 0.0)

        self.HeartImages = {}
        local u1, v1, u2, v2 = HeartUV(0)

        for i = 1, HEART_COUNT do
            local x = (i - 1) * (HEART_DRAW_W + HEART_SPACING)
            local heart = UIManager.CreateImage(self.Panel, x, 0, HEART_DRAW_W, HEART_DRAW_H, HEART_SHEET, nil)
            heart:SetUV(u1, v1, u2, v2)
            heart:SetColor(1.6, 1.6, 1.6, 1.0)
            heart:SetVisible(true)
            self.HeartImages[i] = heart
        end

        self.ProgressBg = UIManager.CreateImage(self.Panel, 0, PROGRESS_Y, PROGRESS_W, PROGRESS_H, PROGRESS_BG_SHEET, nil)
        self.ProgressBg:SetColor(1.0, 1.0, 1.0, 1.0)

        self.ProgressFill = UIManager.CreateImage(self.Panel, 0, PROGRESS_Y, PROGRESS_W, PROGRESS_H, PROGRESS_FILL_SHEET, nil)
        self.ProgressFill:SetColor(1.0, 1.0, 1.0, 1.0)
        ApplyProgressValue(self.ProgressValue)

        local anchorX = -PROGRESS_ANCHOR_OVERLAP
        local anchorY = PROGRESS_Y + (PROGRESS_H - PROGRESS_ANCHOR_SIZE) * 0.5
        self.ProgressAnchor = UIManager.CreateImage(self.Panel, anchorX, anchorY, PROGRESS_ANCHOR_SIZE, PROGRESS_ANCHOR_SIZE, PROGRESS_ANCHOR_SHEET, nil)
        self.ProgressAnchor:SetColor(1.0, 1.0, 1.0, 1.0)

        local timerLabelY = TIMER_Y + (TIMER_ROW_H - TIMER_LABEL_FONT_SIZE) * 0.5
        self.TimerLabel = self.TimeDisplay.CreateSpacedLabel(self.Panel,
            TIMER_LABEL_W * 0.5,
            timerLabelY + TIMER_LABEL_FONT_SIZE * 0.5,
            TIMER_LABEL_TEXT,
            {
                FontSize = TIMER_LABEL_FONT_SIZE,
                CharWidth = 18,
                LetterGap = TIMER_LABEL_LETTER_GAP,
                Mode = "Centered",
                Centered = true,
                Color = { 1.0, 0.95, 0.25, 1.0 },
            })

        self.TimerDisplay = self.TimeDisplay.Create(self.Panel,
            TIMER_LABEL_W + TIMER_GAP,
            TIMER_Y + (TIMER_ROW_H - TIMER_DIGIT_H) * 0.5,
            {
                DigitWidth = TIMER_DIGIT_W,
                DigitHeight = TIMER_DIGIT_H,
                DigitGap = TIMER_DIGIT_GAP,
                SeparatorWidth = TIMER_SEPARATOR_W,
                SeparatorDotSize = 5,
                SeparatorDotGap = 16,
                InitialValue = self.GetElapsedTime(),
                DigitColor = { 1.15, 1.15, 1.15, 1.0 },
                SeparatorColor = { 1.0, 0.95, 0.25, 1.0 },
            })

        self.ShipWheel = UIManager.CreateImage(nil, SHIP_WHEEL_SCREEN_X, SHIP_WHEEL_SCREEN_Y, SHIP_WHEEL_SIZE, SHIP_WHEEL_SIZE, SHIP_WHEEL_SHEET, "RelativePos")
        self.ShipWheel:SetColor(1.0, 1.0, 1.0, 1.0)
        self.ShipWheel:SetRotation(self.ShipWheelAngle)

        self.bHeartAnimPlaying = false
        self.HeartAnimDir = 1
        self.HeartAnimIndex = 0
        self.HeartAnimTime = 0.0
        self.HeartAnimFrame = 0
        self.HeartTargetHealth = self.GetHealth()
        ApplyHeartHealth(self.GetHealth())
        self:Sync()

        self.Minimap:Show()
        self.Minimap:Update()
    end

    function state:Hide()
        self.EnterUIMode()

        if self.Panel then
            UIManager.DestroyElement(self.Panel)
            self.Panel = nil
            self.ProgressBg = nil
            self.ProgressFill = nil
            self.ProgressAnchor = nil
            self.TimerLabel = nil
            self.TimerDisplay = nil
            self.ProgressValue = 0.0
            self.ProgressStartValue = 0.0
            self.ProgressTargetValue = 0.0
            self.ProgressAnimTime = PROGRESS_ANIM_DURATION
            self.HeartImages = {}
            self.bHeartAnimPlaying = false
            self.HeartAnimDir = 1
            self.HeartAnimIndex = 0
            self.HeartAnimTime = 0.0
            self.HeartAnimFrame = 0
            self.HeartHealth = HEART_COUNT
            self.HeartTargetHealth = HEART_COUNT
        end

        if self.ShipWheel then
            UIManager.DestroyElement(self.ShipWheel)
            self.ShipWheel = nil
            self.ShipWheelAngle = 0.0
            self.ShipWheelLastDir = 0
            self.ShipWheelKickTime = SHIP_WHEEL_RELEASE_DURATION
        end

        self.Minimap:Hide()
    end

    function state:Update(deltaTime)
        self:Sync()
        UpdateProgressBar(deltaTime)
        UpdateShipWheel(deltaTime)
        self.Minimap:Update()
        UpdateHeartAnimation(deltaTime)
    end

    return state
end

return GameplayHud
