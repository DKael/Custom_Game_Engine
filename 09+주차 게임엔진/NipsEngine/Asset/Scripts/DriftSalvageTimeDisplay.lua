-- DriftSalvageTimeDisplay.lua
-- Shared Number.png based MM:SS:CC timer display.

local TimeDisplay = {}

local NUMBER_SHEET = "Asset/Texture/UI/Number.png"
local NUMBER_COLS = 5
local NUMBER_ROWS = 2

local function ClampElapsedSeconds(seconds)
    local value = tonumber(seconds) or 0.0
    if value < 0.0 then
        value = 0.0
    end
    return value
end

local function DigitUV(digit)
    local value = math.max(0, math.min(9, math.floor(digit or 0)))
    local col = value % NUMBER_COLS
    local row = math.floor(value / NUMBER_COLS)

    return col / NUMBER_COLS,
        row / NUMBER_ROWS,
        (col + 1) / NUMBER_COLS,
        (row + 1) / NUMBER_ROWS
end

function TimeDisplay.Format(seconds)
    local totalCentiseconds = math.floor(ClampElapsedSeconds(seconds) * 100.0 + 0.5)
    local maxCentiseconds = (99 * 60 + 59) * 100 + 99
    if totalCentiseconds > maxCentiseconds then
        totalCentiseconds = maxCentiseconds
    end

    local minutes = math.floor(totalCentiseconds / 6000)
    local remainder = totalCentiseconds % 6000
    local wholeSeconds = math.floor(remainder / 100)
    local centiseconds = remainder % 100

    return string.format("%02d:%02d:%02d", minutes, wholeSeconds, centiseconds)
end

function TimeDisplay.Create(parent, x, y, options)
    options = options or {}

    local digitW = options.DigitWidth or 28
    local digitH = options.DigitHeight or 44
    local digitGap = options.DigitGap or 2
    local separatorW = options.SeparatorWidth or 14
    local separatorDotSize = options.SeparatorDotSize or math.max(4, math.floor(digitW * 0.18 + 0.5))
    local separatorDotGap = options.SeparatorDotGap or math.max(separatorDotSize * 2.4, digitH * 0.32)
    local separatorYOffset = options.SeparatorYOffset or 0.0
    local mode = options.Mode
    local bCentered = options.Centered == true
    local digitColor = options.DigitColor or { 1.0, 1.0, 1.0, 1.0 }
    local separatorColor = options.SeparatorColor or digitColor

    local totalW = digitW * 6 + separatorW * 2 + digitGap * 7
    local root = UIManager.CreateImage(parent, x, y, totalW, digitH, nil, mode)
    root:SetColor(0.0, 0.0, 0.0, 0.0)

    local state = {
        Root = root,
        DigitImages = {},
        LastText = nil,
    }

    local cursorX = bCentered and -totalW * 0.5 or 0.0
    local baseY = bCentered and -digitH * 0.5 or 0.0

    local function createDigit()
        local image = UIManager.CreateImage(root, cursorX, baseY, digitW, digitH, NUMBER_SHEET, nil)
        image:SetColor(digitColor[1], digitColor[2], digitColor[3], digitColor[4])
        table.insert(state.DigitImages, image)
        cursorX = cursorX + digitW + digitGap
    end

    local function createSeparator()
        local centerX = cursorX + separatorW * 0.5
        local centerY = baseY + digitH * 0.5 + separatorYOffset
        local upperDot = UIManager.CreateImage(root,
            centerX,
            centerY - separatorDotGap * 0.5,
            separatorDotSize,
            separatorDotSize,
            nil,
            "Centered")
        upperDot:SetColor(separatorColor[1], separatorColor[2], separatorColor[3], separatorColor[4])

        local lowerDot = UIManager.CreateImage(root,
            centerX,
            centerY + separatorDotGap * 0.5,
            separatorDotSize,
            separatorDotSize,
            nil,
            "Centered")
        lowerDot:SetColor(separatorColor[1], separatorColor[2], separatorColor[3], separatorColor[4])

        cursorX = cursorX + separatorW + digitGap
    end

    local function createTextSeparator()
        local text = UIManager.CreateText(root,
            cursorX + separatorW * 0.5,
            baseY + digitH * 0.5,
            separatorW,
            digitH,
            ":",
            options.SeparatorFontSize or digitH,
            "Centered")
        text:SetColor(separatorColor[1], separatorColor[2], separatorColor[3], separatorColor[4])
        cursorX = cursorX + separatorW + digitGap
    end

    createDigit()
    createDigit()
    if options.UseTextSeparator then createTextSeparator() else createSeparator() end
    createDigit()
    createDigit()
    if options.UseTextSeparator then createTextSeparator() else createSeparator() end
    createDigit()
    createDigit()

    function state:SetValue(seconds)
        local text = TimeDisplay.Format(seconds)
        if text == self.LastText then
            return
        end

        self.LastText = text
        local digitIndex = 1
        for i = 1, #text do
            local char = string.sub(text, i, i)
            local digit = tonumber(char)
            if digit ~= nil then
                local image = self.DigitImages[digitIndex]
                if image then
                    local u1, v1, u2, v2 = DigitUV(digit)
                    image:SetUV(u1, v1, u2, v2)
                end
                digitIndex = digitIndex + 1
            end
        end
    end

    function state:Destroy()
        if self.Root then
            UIManager.DestroyElement(self.Root)
            self.Root = nil
            self.DigitImages = {}
        end
    end

    state:SetValue(options.InitialValue or 0.0)
    return state
end

function TimeDisplay.CreateSpacedLabel(parent, x, y, text, options)
    options = options or {}

    local value = tostring(text or "")
    local fontSize = options.FontSize or 36
    local charW = options.CharWidth or fontSize * 0.65
    local letterGap = options.LetterGap or fontSize * 0.55
    local wordGap = options.WordGap or fontSize * 1.1
    local mode = options.Mode
    local bCentered = options.Centered ~= false
    local color = options.Color or { 0.0, 0.0, 0.0, 1.0 }

    local totalW = 0.0
    local glyphCount = 0
    for i = 1, #value do
        local char = string.sub(value, i, i)
        if char == " " then
            totalW = totalW + wordGap
        else
            if glyphCount > 0 then
                totalW = totalW + letterGap
            end
            totalW = totalW + charW
            glyphCount = glyphCount + 1
        end
    end

    if totalW <= 0.0 then
        totalW = charW
    end

    local root = UIManager.CreateImage(parent, x, y, totalW, fontSize, nil, mode)
    root:SetColor(0.0, 0.0, 0.0, 0.0)

    local cursorX = bCentered and -totalW * 0.5 or 0.0
    local charY = bCentered and 0.0 or fontSize * 0.5
    for i = 1, #value do
        local char = string.sub(value, i, i)
        if char == " " then
            cursorX = cursorX + wordGap
        else
            local label = UIManager.CreateText(root,
                cursorX + charW * 0.5,
                charY,
                charW,
                fontSize,
                char,
                fontSize,
                "Centered")
            label:SetColor(color[1], color[2], color[3], color[4])
            cursorX = cursorX + charW + letterGap
        end
    end

    return root
end

return TimeDisplay
