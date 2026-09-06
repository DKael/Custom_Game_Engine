-- DriftSalvageHud.lua

local ShowHud

local SCRIPT_DIR = "Asset/Scripts/"
local HUD_BG_SHEET = "Asset/Texture/UI/BG.png"
local SCORE_BG_PATH = "Asset/Texture/UI/ScoreBoardBG.png"
local ENDING_SHEET = "Asset/Texture/UI/Ending.png"
local SCORE_MANAGER_PATH = SCRIPT_DIR .. "ScoreManager.lua"
local TIME_DISPLAY_PATH = SCRIPT_DIR .. "DriftSalvageTimeDisplay.lua"
local MENU_BGM = "Menu.mp3"
local MENU_BGM_VOLUME = 0.1
local INGAME_BGM = "InGame.mp3"
local INGAME_BGM_VOLUME = 0.1
local BOAT_FORWARD_LOOP_SFX = "boatMove.mp3"
local DEFAULT_WEIGHT_CAPACITY = 150.0
local START_FADE_IN_DURATION = 0.55
local RESTART_FADE_OUT_DURATION = 0.55
local PLAYER_CAMERA_TARGET = "PlayerCamera"
local INTRO_CAMERA_TARGET = "IntroCamera"
local START_CAMERA_BLEND_TIME = 1.5
local INTRO_LETTERBOX_HEIGHT = 0.1
local INTRO_LETTERBOX_BLEND_IN = 0.5
local INTRO_LETTERBOX_DURATION = 3.0
local GAME_OVER_CAMERA_TARGET = "GameOverCamera"
local GAME_OVER_CAMERA_BLEND_TIME = 3.0
local GAME_OVER_CAMERA_BACK_DISTANCE = 7.0
local GAME_OVER_CAMERA_SIDE_OFFSET = 1.5
local GAME_OVER_CAMERA_HEIGHT = 30.0
local GAME_OVER_CAMERA_LOOK_HEIGHT = 0.6
local GAME_OVER_CAMERA_FORCE_PERSPECTIVE = true

local MenuPanel = nil
local ScorePanel = nil
local GameOverPanel = nil
local BoatActor = nil
local BoatHomePosition = nil
local BoatHomeRotation = nil
local ScoreManager = nil
local TimeDisplay = nil
local GameplayHud = nil
local Countdown = nil
local bGameplayInputReady = false
local GameplayElapsedTime = 0.0
local FinalElapsedTime = nil
local bGameplayTimerRunning = false
local bLowHealthVignetteActive = false
local prevHealth = nil
local LOW_HEALTH_VIGNETTE_RADIUS = 5.0
local LOW_HEALTH_VIGNETTE_BLEND_TIME = 1.0
local DAMAGE_SHAKE_DURATION = 0.3
local DAMAGE_SHAKE_LOCATION_AMPLITUDE = 4.0
local DAMAGE_SHAKE_LOCATION_FREQUENCY = 3.0
local bGameOverSequencePlaying = false
local bRestartFadeOutPending = false
local bLighthouseEndingPlaying = false
local BOAT_SINK_GAMEOVER_DELAY = 3.0
local BOAT_SINK_DEPTH = 4.0
local BOAT_SINK_ROLL_DEGREES = 12.0
local BOAT_SINK_YAW_DEGREES = 24.0
local DROWNING_SFX = "Drowning.wav"
local WAVE_SFX = "Wave.mp3"

-- Lighthouse 도착 엔딩 연출 파라미터.
-- 카메라가 CineCamera_Outro로 블렌딩되는 동안 보트는 선형 감속으로 정지한다.
-- 진행 거리 = 0.5 * LIGHTHOUSE_COAST_INITIAL_SPEED * LIGHTHOUSE_COAST_DURATION.
local LIGHTHOUSE_END_CAMERA_TAG    = "CineCamera_Outro"
local LIGHTHOUSE_GOAL_SFX          = "GoalIn.mp3"
local LIGHTHOUSE_GOAL_SFX_VOLUME   = 1.0
local LIGHTHOUSE_CAMERA_BLEND_TIME = 3.0
-- 보트가 "들어오는" 연출 시간. v0=18·t=3 → 약 27 unit 전진하며 3초간 감속해 정지.
local LIGHTHOUSE_COAST_DISTANCE      = 40.0
local LIGHTHOUSE_COAST_DURATION      = 3.0
-- 보트가 정지한 뒤 GameOver UI를 띄우기까지 비워두는 여백 시간.
local LIGHTHOUSE_POST_STOP_HOLD      = 1.2
local LIGHTHOUSE_SLOMO_SCALE         = 0.4
local LIGHTHOUSE_SLOMO_DURATION      = 1.5

local function LoadScript(path)
    local paths = {
        path,
        "NipsEngine/" .. path,
    }

    local lastError = nil
    for _, candidate in ipairs(paths) do
        local ok, result = pcall(dofile, candidate)
        if ok and result then
            return result
        end
        lastError = result
    end

    error("Lua script load failed: " .. tostring(path) .. " (" .. tostring(lastError) .. ")")
end

local function LoadScoreManager()
    if ScoreManager then
        return ScoreManager
    end

    local ok, managerOrError = pcall(LoadScript, SCORE_MANAGER_PATH)
    if ok and managerOrError then
        ScoreManager = managerOrError
        return ScoreManager
    end

    if Log then
        Log("ScoreManager load failed: " .. tostring(managerOrError))
    end

    ScoreManager = {
        GetRecords = function()
            return { 0, 0, 0 }
        end,
        RecordTime = function(seconds)
            local time = math.max(0.0, tonumber(seconds) or 0.0)
            local records = { time, 0, 0 }
            table.sort(records, function(a, b)
                if a <= 0.0 and b <= 0.0 then return false end
                if a <= 0.0 then return false end
                if b <= 0.0 then return true end
                return a < b
            end)
            return records
        end,
    }
    ScoreManager.RecordScore = ScoreManager.RecordTime
    return ScoreManager
end

local function LoadTimeDisplay()
    if TimeDisplay then
        return TimeDisplay
    end

    TimeDisplay = LoadScript(TIME_DISPLAY_PATH)
    return TimeDisplay
end

local function Clamp01(value)
    return math.max(0.0, math.min(1.0, value or 0.0))
end

local function EaseSmoothStep(t)
    t = Clamp01(t)
    return t * t * (3.0 - 2.0 * t)
end

local function Lerp(a, b, t)
    return a + (b - a) * t
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

local function StopBoatForwardLoopSfx()
    if Sound and Sound.StopSFX then
        Sound.StopSFX(BOAT_FORWARD_LOOP_SFX)
    end
end

local function FadeInGameplay()
    if Camera and Camera.FadeIn then
        Camera.FadeIn(START_FADE_IN_DURATION)
    end
end

local function ReturnToPlayerCamera(blendTime)
    if Camera and Camera.SetViewTargetWithBlend then
        Camera.SetViewTargetWithBlend(PLAYER_CAMERA_TARGET, blendTime or 0.0)
    end
end

local function SetIntroCamera(blendTime)
    if Camera and Camera.SetViewTargetWithBlend then
        Camera.SetViewTargetWithBlend(INTRO_CAMERA_TARGET, blendTime or 0.0)
    end
end

local function RequestGameplayRestartWithFadeOut()
    if bRestartFadeOutPending then
        return
    end

    bRestartFadeOutPending = true
    bGameplayInputReady = false
    StopBoatForwardLoopSfx()

    if Sound and Sound.StopSFX then
        Sound.StopSFX(DROWNING_SFX)
    end

    if Camera and Camera.FadeOut then
        Camera.FadeOut(RESTART_FADE_OUT_DURATION)
    end

    local function restart()
        _G.DriftSalvageHudNextStartMode = "Gameplay"
        RequestGameRestart()
    end

    if StartCoroutine and Wait then
        StartCoroutine(function()
            Wait(RESTART_FADE_OUT_DURATION)
            restart()
        end)
    else
        restart()
    end
end

local function GetHudHealth()
    if GetDriftSalvageHealth then
        return GetDriftSalvageHealth()
    end

    return 5
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

local function GetTimerDeltaTime(fallbackDeltaTime)
    local dt = fallbackDeltaTime or 0.0
    if TimeManager and TimeManager.GetUnscaledDeltaTime then
        dt = TimeManager.GetUnscaledDeltaTime()
    end

    if dt < 0.0 then
        return 0.0
    end
    return dt
end

local function ResetGameplayTimer()
    GameplayElapsedTime = 0.0
    FinalElapsedTime = nil
    bGameplayTimerRunning = false
end

local function StartGameplayTimer()
    GameplayElapsedTime = 0.0
    FinalElapsedTime = nil
    bGameplayTimerRunning = true
end

local function GetGameplayElapsedTime()
    return FinalElapsedTime or GameplayElapsedTime
end

local function StopGameplayTimer()
    if not FinalElapsedTime then
        FinalElapsedTime = GameplayElapsedTime
    end

    bGameplayTimerRunning = false
    return FinalElapsedTime
end

local function TickGameplayTimer(deltaTime)
    if not bGameplayTimerRunning then
        return
    end

    GameplayElapsedTime = GameplayElapsedTime + GetTimerDeltaTime(deltaTime)
end

local function SetLowHealthVignette(enabled)
    if bLowHealthVignetteActive == enabled then
        return
    end

    bLowHealthVignetteActive = enabled

    if not Camera or not Camera.SetVignette then
        return
    end

    if enabled then
        Camera.SetVignette(0.55, LOW_HEALTH_VIGNETTE_RADIUS, 0.35, 1.0, 0.0, 0.0, LOW_HEALTH_VIGNETTE_BLEND_TIME)
    else
        Camera.SetVignette(0.0, LOW_HEALTH_VIGNETTE_RADIUS, 0.35, 0.0, 0.0, 0.0, LOW_HEALTH_VIGNETTE_BLEND_TIME)
    end
end

local function SetGameOverVignette()
    bLowHealthVignetteActive = true
    if not Camera or not Camera.SetVignette then
        return
    end
    Camera.SetVignette(0.85, LOW_HEALTH_VIGNETTE_RADIUS, 0.35, 0.0, 0.0, 0.0, LOW_HEALTH_VIGNETTE_BLEND_TIME)
end

local function FlashHitVignette()
    if not Camera or not Camera.SetVignette then
        return
    end
    Camera.SetVignette(0.55, LOW_HEALTH_VIGNETTE_RADIUS, 0.35, 1.0, 0.0, 0.0, 0.1)
    StartCoroutine(function()
        Wait(0.5)
        if not bLowHealthVignetteActive then
            Camera.SetVignette(0.0, LOW_HEALTH_VIGNETTE_RADIUS, 0.35, 1.0, 0.0, 0.0, LOW_HEALTH_VIGNETTE_BLEND_TIME)
        end
    end)
end

local function PlayDamageCameraShake()
    if not Camera or not Camera.Shake then
        return
    end

    Camera.Shake({
        Type = "WaveOscillator",
        Duration = DAMAGE_SHAKE_DURATION,
        LocationAmplitude = Vec3.new(
            DAMAGE_SHAKE_LOCATION_AMPLITUDE,
            DAMAGE_SHAKE_LOCATION_AMPLITUDE,
            DAMAGE_SHAKE_LOCATION_AMPLITUDE * 0.4),
        LocationFrequency = Vec3.new(
            DAMAGE_SHAKE_LOCATION_FREQUENCY,
            DAMAGE_SHAKE_LOCATION_FREQUENCY * 1.15,
            DAMAGE_SHAKE_LOCATION_FREQUENCY * 0.85),
        RotationAmplitude = Vec3.new(0.0, 0.0, 0.0),
        RotationFrequency = Vec3.new(0.0, 0.0, 0.0),
        FOVAmplitude = 0.0,
        FOVFrequency = 0.0,
    })
end

local function UpdateLowHealthVignette()
    local currentHealth = GetHudHealth()
    if prevHealth ~= nil and currentHealth < prevHealth then
        FlashHitVignette()
        PlayDamageCameraShake()
    end
    prevHealth = currentHealth
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

local function GetPlanarForward(actor)
    if not actor or not actor.GetForwardVector then
        return 1.0, 0.0
    end

    local forward = actor:GetForwardVector()
    local x = forward and (forward.X or 0.0) or 0.0
    local y = forward and (forward.Y or 0.0) or 0.0
    local length = math.sqrt(x * x + y * y)

    if length <= 0.0001 then
        return 1.0, 0.0
    end

    return x / length, y / length
end

local function BeginGameOverCameraTransition(boat, focusPos)
    if not Camera or not Camera.SetViewTargetWithBlend then
        return
    end

    local cameraActor = nil
    if FindActorByTagOrName then
        cameraActor = FindActorByTagOrName(GAME_OVER_CAMERA_TARGET)
    elseif FindActorByTag then
        cameraActor = FindActorByTag(GAME_OVER_CAMERA_TARGET)
    end

    if not cameraActor or not cameraActor:IsValid() then
        if SpawnCameraActor then
            cameraActor = SpawnCameraActor(GAME_OVER_CAMERA_TARGET)
        end

        if not cameraActor or not cameraActor:IsValid() then
            if Log then
                Log("GameOverCamera not found and SpawnCameraActor is unavailable.")
            end
            return
        end
    end

    local focus = focusPos
    if not focus and boat and boat:IsValid() then
        focus = boat:GetPosition()
    end
    if not focus then
        return
    end

    local forwardX, forwardY = GetPlanarForward(boat)
    local rightX = -forwardY
    local rightY = forwardX
    local cameraX = (focus.X or 0.0) - forwardX * GAME_OVER_CAMERA_BACK_DISTANCE + rightX * GAME_OVER_CAMERA_SIDE_OFFSET
    local cameraY = (focus.Y or 0.0) - forwardY * GAME_OVER_CAMERA_BACK_DISTANCE + rightY * GAME_OVER_CAMERA_SIDE_OFFSET
    local cameraZ = (focus.Z or 0.0) + GAME_OVER_CAMERA_HEIGHT

    cameraActor:SetPosition(cameraX, cameraY, cameraZ)

    local cameraComponent = nil
    if cameraActor.FindCameraComponent then
        cameraComponent = cameraActor:FindCameraComponent()
    end

    if cameraComponent then
        if GAME_OVER_CAMERA_FORCE_PERSPECTIVE and cameraComponent.SetOrthographic then
            cameraComponent:SetOrthographic(false)
        end

        if cameraComponent.SetWorldLocation then
            cameraComponent:SetWorldLocation(cameraX, cameraY, cameraZ)
        end

        if cameraComponent.LookAt then
            cameraComponent:LookAt(focus.X or 0.0, focus.Y or 0.0, (focus.Z or 0.0) + GAME_OVER_CAMERA_LOOK_HEIGHT)
        end
    elseif Log then
        Log("GameOverCamera has no CameraComponent.")
    end

    Camera.SetViewTargetWithBlend(GAME_OVER_CAMERA_TARGET, GAME_OVER_CAMERA_BLEND_TIME)
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
    ReturnToPlayerCamera(0.0)
end

local function CreateCenteredText(parent, x, y, text, fontSize)
    local value = tostring(text or "")
    local size = fontSize or 16.0
    return UIManager.CreateText(parent, x, y, math.max(size, #value * size), size, value, size, "Centered")
end

local function MakeHoverLabel(parent, x, y, w, h, text, fontSize, mode)
    local label = UIManager.CreateText(parent, x, y, w, h, text, fontSize, mode)
    label:SetInteractable(true)
    label:OnHoverEnter(function() label:SetColor(1.0, 1.0, 0.0, 1.0) end)
    label:OnHoverExit(function() label:SetColor(0.0, 0.0, 0.0, 1.0) end)
    return label
end

local function GetGameplayHud()
    if GameplayHud then
        return GameplayHud
    end

    local MinimapModule = LoadScript(SCRIPT_DIR .. "DriftSalvageMinimap.lua")
    local GameplayHudModule = LoadScript(SCRIPT_DIR .. "DriftSalvageGameplayHud.lua")
    local minimap = MinimapModule.Create({
        ResolveBoatActor = ResolveBoatActor,
        GetActorYawDegrees = GetActorYawDegrees,
    })

    GameplayHud = GameplayHudModule.Create({
        Minimap = minimap,
        TimeDisplay = LoadTimeDisplay(),
        Clamp01 = Clamp01,
        EaseSmoothStep = EaseSmoothStep,
        EnterUIMode = EnterUIMode,
        GetElapsedTime = GetGameplayElapsedTime,
        GetWeight = GetHudWeight,
        GetWeightCapacity = GetHudWeightCapacity,
        GetHealth = GetHudHealth,
    })

    return GameplayHud
end

local function GetCountdown()
    if Countdown then
        return Countdown
    end

    local CountdownModule = LoadScript(SCRIPT_DIR .. "DriftSalvageCountdown.lua")
    Countdown = CountdownModule.Create({
        OnFinished = function()
            bGameplayInputReady = true
            StartGameplayTimer()
            UpdateLowHealthVignette()
            if Sound and Sound.PlayBGM then
                Sound.PlayBGM(INGAME_BGM, INGAME_BGM_VOLUME)
            end
            EnterGameplayMode()
        end,
    })

    return Countdown
end

local function HideHud()
    if MenuPanel then
        UIManager.DestroyElement(MenuPanel)
        MenuPanel = nil
    end
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

local function HideCountdown()
    if Countdown then
        Countdown:Hide()
    end
end

local function HideInGameHud()
    if GameplayHud then
        GameplayHud:Hide()
    end
end

local function ShowInGameHud()
    GetGameplayHud():Show()
end

local function ShowScoreboard(records)
    if ScorePanel then return end

    ScorePanel = UIManager.CreateImage(nil, 0.5, 0.5, 448, 558, SCORE_BG_PATH, "RelativePos")
    ScorePanel:SetColor(1.0, 1.0, 1.0, 1.0)

    local timeDisplay = LoadTimeDisplay()
    timeDisplay.CreateSpacedLabel(ScorePanel, 0, -186, "TIME RECORD", {
        FontSize = 28,
        CharWidth = 16,
        LetterGap = 14,
        WordGap = 34,
        Mode = "Centered",
        Centered = true,
        Color = { 0.0, 0.0, 0.0, 1.0 },
    })

    if type(records) ~= "table" then
        records = LoadScoreManager().GetRecords()
    end

    local rowY = { -104, -14, 76 }
    for rank = 1, 3 do
        local recordTime = tonumber(records[rank]) or 0.0
        if recordTime > 0.0 then
            timeDisplay.Create(ScorePanel, 44, rowY[rank], {
                Mode = "Centered",
                Centered = true,
                DigitWidth = 34,
                DigitHeight = 48,
                DigitGap = 5,
                SeparatorWidth = 16,
                SeparatorDotSize = 7,
                SeparatorDotGap = 18,
                InitialValue = recordTime,
                DigitColor = { 0.0, 0.0, 0.0, 1.0 },
                SeparatorColor = { 0.0, 0.0, 0.0, 1.0 },
            })
        else
            local emptyText = CreateCenteredText(ScorePanel, 44, rowY[rank], "--:--:--", 42.0)
            emptyText:SetColor(0.0, 0.0, 0.0, 1.0)
        end
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

local function ShowGameOver(bRecordFinishTime)
    if GameOverPanel then return end

    local finalTime = StopGameplayTimer()
    bGameplayInputReady = false
    bGameOverSequencePlaying = false
    bLighthouseEndingPlaying = false
    ResetGameplayTimer()
    StopBoatForwardLoopSfx()
    SetLowHealthVignette(false)
    EnterUIMode()
    if bRecordFinishTime then
        LoadScoreManager().RecordTime(finalTime)
    end
    HideInGameHud()
    HideCountdown()

    GameOverPanel = UIManager.CreateImage(nil, 0.5, 0.5, 0.55, 0.55, nil, "FullRelative")
    GameOverPanel:SetColor(0.0, 0.0, 0.0, 0.0)

    local gameOverBg = UIManager.CreateImage(GameOverPanel, 0.0, 0.0, 0.75, 1.5, HUD_BG_SHEET, "ParentRelative")
    gameOverBg:SetColor(1.0, 1.0, 1.0, 1.0)

    local endingImage = UIManager.CreateImage(GameOverPanel, 0.0, -0.34, 0.46, 0.42, ENDING_SHEET, "ParentRelative")
    endingImage:SetColor(1.0, 1.0, 1.0, 1.0)

    LoadTimeDisplay().CreateSpacedLabel(GameOverPanel, 0.0, -0.06, "TIME", {
        FontSize = 38,
        CharWidth = 20,
        LetterGap = 18,
        Mode = "RelativePos",
        Centered = true,
        Color = { 0.0, 0.0, 0.0, 1.0 },
    })

    LoadTimeDisplay().Create(GameOverPanel, 0.0, 0.03, {
        Mode = "RelativePos",
        Centered = true,
        DigitWidth = 42,
        DigitHeight = 58,
        DigitGap = 6,
        SeparatorWidth = 18,
        SeparatorDotSize = 8,
        SeparatorDotGap = 22,
        InitialValue = finalTime,
        DigitColor = { 0.0, 0.0, 0.0, 1.0 },
        SeparatorColor = { 0.0, 0.0, 0.0, 1.0 },
    })

    local restartLabel = UIManager.CreateText(GameOverPanel, -0.01, 0.15, 280, 48, "RESTART", 48.0, "RelativePos")
    restartLabel:SetColor(0.0, 0.0, 0.0, 1.0)
    restartLabel:SetInteractable(true)
    restartLabel:OnHoverEnter(function() restartLabel:SetColor(1.0, 1.0, 0.0, 1.0) end)
    restartLabel:OnHoverExit(function() restartLabel:SetColor(0.0, 0.0, 0.0, 1.0) end)
    restartLabel:OnClick(function()
        RequestGameplayRestartWithFadeOut()
    end)

    local titleLabel = UIManager.CreateText(GameOverPanel, 0.01, 0.27, 280, 48, "TITLE", 48.0, "RelativePos")
    titleLabel:SetColor(0.0, 0.0, 0.0, 1.0)
    titleLabel:SetInteractable(true)
    titleLabel:OnHoverEnter(function() titleLabel:SetColor(1.0, 1.0, 0.0, 1.0) end)
    titleLabel:OnHoverExit(function() titleLabel:SetColor(0.0, 0.0, 0.0, 1.0) end)
    titleLabel:OnClick(function()
        if Sound and Sound.StopSFX then
            Sound.StopSFX(DROWNING_SFX)
        end
        _G.DriftSalvageHudNextStartMode = "Title"
        RequestGameRestart()
    end)
end

local function StartGameplay(playerCameraBlendTime)
    bRestartFadeOutPending = false
    bGameplayInputReady = false
    bGameOverSequencePlaying = false
    bLighthouseEndingPlaying = false
    ResetGameplayTimer()
    StopBoatForwardLoopSfx()
    SetLowHealthVignette(false)
    ReturnToPlayerCamera(playerCameraBlendTime or 0.0)
    FadeInGameplay()
    HideHud()
    HideCountdown()
    if Sound and Sound.StopBGM then
        Sound.StopBGM()
    end
    ShowInGameHud()
    if ResetDriftSalvageStats then
        ResetDriftSalvageStats()
    end
    SetBoatAtHome()
    EnterUIMode()
    StartCoroutine(function()
        GetCountdown():Run()
    end)
end

ShowHud = function()
    if MenuPanel then return end

    bRestartFadeOutPending = false
    bGameplayInputReady = false
    bGameOverSequencePlaying = false
    bLighthouseEndingPlaying = false
    ResetGameplayTimer()
    StopBoatForwardLoopSfx()
    SetLowHealthVignette(false)
    if Sound and Sound.PlayBGM then
        Sound.PlayBGM(MENU_BGM, MENU_BGM_VOLUME)
    end
    HideInGameHud()
    HideGameOver()
    HideCountdown()

    MenuPanel = UIManager.CreateImage(nil, 0.5, 0.5, 0.4, 0.5, nil, "FullRelative")
    MenuPanel:SetColor(0.0, 0.0, 0.0, 0.0)

    local menuBg = UIManager.CreateImage(MenuPanel, 0.0, 0.0, 1.0, 1.5, HUD_BG_SHEET, "ParentRelative")
    menuBg:SetColor(1.0, 1.0, 1.0, 1.0)

    UIManager.CreateImage(MenuPanel, 0.0, -0.3, 0.8, 0.3, "Asset/Texture/UI/Logo.png", "ParentRelative")
    UIManager.CreateImage(MenuPanel, -0.25, 0.1, 0.15, 0.25, "Asset/Texture/UI/Icon_boat.png", "ParentRelative")

    local startLabel = MakeHoverLabel(MenuPanel, 0.025, 0.03, 256, 28, "START", 48.0, "RelativePos")
    startLabel:OnClick(function()
        StartCoroutine(function()
            HideHud()
            if Sound and Sound.StopBGM then
                Sound.StopBGM()
            end
            SetIntroCamera(0.0)
            if Camera and Camera.SetLetterBox then
                Camera.SetLetterBox(INTRO_LETTERBOX_HEIGHT, INTRO_LETTERBOX_BLEND_IN)
            end
            if Sound and Sound.PlaySFX then
                Sound.PlaySFX(WAVE_SFX, 1.0)
            end
            Wait(INTRO_LETTERBOX_DURATION)
            ReturnToPlayerCamera(START_CAMERA_BLEND_TIME)
            if Camera and Camera.SetLetterBox then
                Camera.SetLetterBox(0.0, START_CAMERA_BLEND_TIME)
            end
            Wait(START_CAMERA_BLEND_TIME)
            StartGameplay(0.0)
        end)
    end)

    UIManager.CreateImage(MenuPanel, -0.25, 0.4, 0.15, 0.25, "Asset/Texture/UI/Icon_book.png", "ParentRelative")

    local recordLabel = MakeHoverLabel(MenuPanel, 0.025, 0.18, 256, 28, "RECORD", 48.0, "RelativePos")
    recordLabel:OnClick(function()
        HideHud()
        ShowScoreboard(LoadScoreManager().GetRecords())
    end)

    local ok, err = pcall(PrepareMenuPresentation)
    if not ok and Log then
        Log("PrepareMenuPresentation failed: " .. tostring(err))
    end
end

function OnStart(self)
    local nextStartMode = _G.DriftSalvageHudNextStartMode
    _G.DriftSalvageHudNextStartMode = nil

    if nextStartMode == "Gameplay" then
        StartGameplay(0.0)
        return
    end

    ShowHud()
end

local function StartHealthZeroGameOverSequence()
    if bGameOverSequencePlaying or GameOverPanel then
        return
    end

    bGameOverSequencePlaying = true
    bGameplayInputReady = false
    StopGameplayTimer()
    StopBoatForwardLoopSfx()
    SetGameOverVignette()
    EnterUIMode()

    StartCoroutine(function()
        local boat = ResolveBoatActor()
        local startTime = GetTimeSeconds and GetTimeSeconds() or 0.0
        local startPos = nil
        local startRot = nil

        if boat and boat:IsValid() then
            startPos = boat:GetPosition()
            startRot = boat:GetRotation()
        end

        BeginGameOverCameraTransition(boat, startPos)

        if Sound and Sound.PlaySFX then
            Sound.PlaySFX(DROWNING_SFX, 3.0)
        end

        while true do
            local now = GetTimeSeconds and GetTimeSeconds() or (startTime + BOAT_SINK_GAMEOVER_DELAY)
            local alpha = Clamp01((now - startTime) / BOAT_SINK_GAMEOVER_DELAY)
            local eased = EaseSmoothStep(alpha)

            if boat and boat:IsValid() and startPos and startRot then
                boat:SetPosition(
                    startPos.X or 0.0,
                    startPos.Y or 0.0,
                    Lerp(startPos.Z or 0.0, (startPos.Z or 0.0) - BOAT_SINK_DEPTH, eased))
                boat:SetRotation(
                    Lerp(startRot.X or 0.0, (startRot.X or 0.0) + BOAT_SINK_ROLL_DEGREES, eased),
                    startRot.Y or 0.0,
                    Lerp(startRot.Z or 0.0, (startRot.Z or 0.0) + BOAT_SINK_YAW_DEGREES, eased))
            end

            if alpha >= 1.0 then
                break
            end

            Wait(0.0)
        end

        ShowGameOver(false)
    end)
end

-- Boat가 Lighthouse Sphere에 진입하면 C++ TriggerBoatLighthouseGameOver가
-- LuaBinder::RequestLighthouseEnding()을 호출 → OnUpdate가 ConsumeLighthouseEndingRequest로
-- 이 시퀀스를 발화한다. 침몰 시퀀스와 달리 보트는 전방으로 미끄러져 정지한다.
local function StartLighthouseEndingSequence()
    if bLighthouseEndingPlaying or bGameOverSequencePlaying or GameOverPanel then
        return
    end

    bLighthouseEndingPlaying = true
    bGameplayInputReady = false
    StopGameplayTimer()
    if GameplayHud and GameplayHud:IsVisible() then
        GameplayHud:Sync()
    end

    if Input and Input.SetPlayerControlEnabled then
        Input.SetPlayerControlEnabled(false)
    end

    StopBoatForwardLoopSfx()
    EnterUIMode()

    if Sound and Sound.PlaySFX then
        Sound.PlaySFX(LIGHTHOUSE_GOAL_SFX, LIGHTHOUSE_GOAL_SFX_VOLUME, false)
    end

    if Camera and Camera.SetViewTargetWithBlend then
        Camera.SetViewTargetWithBlend(LIGHTHOUSE_END_CAMERA_TAG, LIGHTHOUSE_CAMERA_BLEND_TIME)
    end
    if HitFeel and HitFeel.Slomo then
        HitFeel.Slomo(LIGHTHOUSE_SLOMO_SCALE, LIGHTHOUSE_SLOMO_DURATION)
    end

    StartCoroutine(function()
        local boat = ResolveBoatActor()
        if boat and boat:IsValid() then
            local elapsed = 0.0
            local traveled = 0.0
            while elapsed < LIGHTHOUSE_COAST_DURATION do
                if not boat:IsValid() then
                    break
                end

                local dt = 0.016
                if TimeManager and TimeManager.GetUnscaledDeltaTime then
                    dt = TimeManager.GetUnscaledDeltaTime()
                elseif TimeManager and TimeManager.GetScaledDeltaTime then
                    dt = TimeManager.GetScaledDeltaTime()
                end
                if dt < 0.0 then dt = 0.0 end

                elapsed = elapsed + dt
                local t = elapsed / LIGHTHOUSE_COAST_DURATION
                if t > 1.0 then t = 1.0 end
                local eased = 1.0 - (1.0 - t) * (1.0 - t)
                local nextTraveled = LIGHTHOUSE_COAST_DISTANCE * eased
                local step = nextTraveled - traveled
                traveled = nextTraveled

                boat:AddPosition(step, 0.0, 0.0)

                Wait(0.0)
            end
        end

        -- 보트가 완전히 멈춘 직후 곧바로 GameOver UI를 띄우면 정적이 너무 짧아 부자연스럽다.
        -- 의도적으로 잠깐 비워둬 카메라/엔딩 컷 분위기를 살린 뒤 UI를 노출한다.
        if LIGHTHOUSE_POST_STOP_HOLD > 0.0 then
            Wait(LIGHTHOUSE_POST_STOP_HOLD)
        end

        ShowGameOver(true)
    end)
end

function OnUpdate(self, deltaTime)
    local hud = GameplayHud
    if not hud then
        return
    end

    -- Lighthouse 엔딩은 침몰 시퀀스를 우회하므로 가장 먼저 검사한다.
    if bGameplayInputReady and hud:IsVisible() then
        TickGameplayTimer(deltaTime)
    end

    if hud:IsVisible() and ConsumeLighthouseEndingRequest and ConsumeLighthouseEndingRequest() then
        StartLighthouseEndingSequence()
        return
    end

    -- Lighthouse 엔딩이 진행 중이면 다른 GameOver 트리거(체력 0 등)는 무시한다.
    if bLighthouseEndingPlaying then
        return
    end

    if bGameplayInputReady and hud:IsVisible() and ConsumeDriftSalvageGameOverRequest and ConsumeDriftSalvageGameOverRequest() then
        StartHealthZeroGameOverSequence()
        return
    end

    if bGameplayInputReady and hud:IsVisible() and Input and Input.GetKeyDown("E") then
        ShowGameOver(false)
        return
    end

    hud:Update(deltaTime)

    if bGameplayInputReady and hud:IsVisible() then
        UpdateLowHealthVignette()
    end

    if bGameplayInputReady and hud:IsVisible() and GetHudHealth() <= 0 and not hud:IsHeartAnimating() then
        StartHealthZeroGameOverSequence()
    end
end

function OnDestroy(self)
    bGameplayInputReady = false
    bGameOverSequencePlaying = false
    bLighthouseEndingPlaying = false
    ResetGameplayTimer()
    StopBoatForwardLoopSfx()
    SetLowHealthVignette(false)
    EnterUIMode()
    HideHud()
    HideScoreboard()
    HideGameOver()
    HideInGameHud()
    HideCountdown()
end
