-- DriftSalvageCountdown.lua

local Countdown = {}

local NUMBER_SHEET = "Asset/Texture/UI/Number.png"
local NUMBER_COLS = 5
local NUMBER_ROWS = 2
local COUNTDOWN_SOUND = "CountDown.wav"
local COUNTDOWN_SOUND_VOLUME = 0.05
local COUNTDOWN_SIZE = 220
local COUNTDOWN_X = 0.5
local COUNTDOWN_Y = 0.45
local STEP_SECONDS = 1.0

local function DigitUV(digit)
    local value = math.max(0, math.min(9, math.floor(digit or 0)))
    local col = value % NUMBER_COLS
    local row = math.floor(value / NUMBER_COLS)

    return col / NUMBER_COLS,
        row / NUMBER_ROWS,
        (col + 1) / NUMBER_COLS,
        (row + 1) / NUMBER_ROWS
end

function Countdown.Create(deps)
    local state = {
        Image = nil,
        OnFinished = deps.OnFinished,
    }

    function state:Hide()
        if self.Image then
            UIManager.DestroyElement(self.Image)
            self.Image = nil
        end
    end

    function state:ShowDigit(digit)
        if not self.Image then
            self.Image = UIManager.CreateImage(nil,
                COUNTDOWN_X, COUNTDOWN_Y,
                COUNTDOWN_SIZE, COUNTDOWN_SIZE,
                NUMBER_SHEET, "RelativePos")
            self.Image:SetColor(1.0, 1.0, 1.0, 1.0)
        end

        local u1, v1, u2, v2 = DigitUV(digit)
        self.Image:SetUV(u1, v1, u2, v2)
    end

    function state:Run()
        if Sound and Sound.PlaySFX then
            Sound.PlaySFX(COUNTDOWN_SOUND, COUNTDOWN_SOUND_VOLUME)
        end

        for digit = 3, 1, -1 do
            self:ShowDigit(digit)
            Wait(STEP_SECONDS)
        end

        self:Hide()

        if self.OnFinished then
            self.OnFinished()
        end
    end

    return state
end

return Countdown
