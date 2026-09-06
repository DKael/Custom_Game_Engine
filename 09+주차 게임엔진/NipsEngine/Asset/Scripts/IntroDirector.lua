-- Tag one ACineCameraActor as "CineCamera_Intro".
-- "PlayerCamera" resolves back to the first pawn camera.

local ActiveText = nil
local WAVE_SFX = "Wave.mp3"
local WAVE_SFX_VOLUME = 0.5

local function PlayWaveSfx()
    if Sound and Sound.PlaySFX then
        Sound.PlaySFX(WAVE_SFX, WAVE_SFX_VOLUME)
    end
end

local function ClearText()
    if ActiveText ~= nil then
        UIManager.DestroyElement(ActiveText)
        ActiveText = nil
    end
end

local function ShowText(text, duration, y)
    ClearText()

    ActiveText = UIManager.CreateText(nil, 0.36, y or 0.14, 900, 56, text, 48, "RelativePos")
    ActiveText:SetColor(1.0, 1.0, 1.0, 1.0)

    if duration ~= nil and duration > 0.0 then
        Wait(duration)
        ClearText()
    end
end

local function IntroSequence()
    if Input and Input.SetPlayerControlEnabled then
        Input.SetPlayerControlEnabled(false)
    end

    Camera.EnableGammaCorrection(true)
    PlayWaveSfx()
    Camera.SetLetterBox(0.14, 0.5)
    Camera.FadeIn(2.0)
    Camera.SetViewTargetWithBlend("CineCamera_Intro", 0.0)
    Wait(2.0)

    ShowText("Team 7", 2.0, 0.12)
    ShowText("Member A / Member B / Member C", 3.0, 0.18)

    Camera.SetViewTargetWithBlend("PlayerCamera", 1.5)
    Wait(1.5)

    Camera.SetLetterBox(0.0, 1.0)
    ClearText()

    if Input and Input.SetPlayerControlEnabled then
        Input.SetPlayerControlEnabled(true)
    end
end

function BeginPlay(self)
    StartCoroutine(IntroSequence)
end

function OnDestroy(self)
    ClearText()
end
