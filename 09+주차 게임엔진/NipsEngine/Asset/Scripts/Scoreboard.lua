local SCORE_BG_PATH = "Asset/Texture/UI/ScoreBoardBG.png"


local ScorePanel = nil

local function CreateCenteredText(parent, x, y, text, fontSize)
    local value = tostring(text or "")
    local size = fontSize or 16.0
    return UIManager.CreateText(parent, x, y, math.max(size, #value * size), size, value, size, "Centered")
end

function ShowScoreboard(dummyScore)
    if ScorePanel then return end

    ScorePanel = UIManager.CreateImage(nil, 0.5, 0.5, 448, 558, SCORE_BG_PATH, "RelativePos")
    ScorePanel:SetColor(1.0, 1.0, 1.0, 1.0)

    local title = CreateCenteredText(ScorePanel, 0, -186, "RECORD", 36.0)
    title:SetColor(0.0, 0.0, 0.0, 1.0)

    local scoreText = CreateCenteredText(ScorePanel, 0, 0, math.max(0, math.floor(dummyScore or 0)), 40.0)
    scoreText:SetColor(0.0, 0.0, 0.0, 1.0)

    local closeBtn = CreateCenteredText(ScorePanel, 0, 226, "CLOSE", 30.0)
    closeBtn:SetColor(0.0, 0.0, 0.0, 1.0)
    closeBtn:SetInteractable(true)
    closeBtn:OnHoverEnter(function() closeBtn:SetColor(1.0, 1.0, 0.0, 1.0) end)
    closeBtn:OnHoverExit(function() closeBtn:SetColor(0.0, 0.0, 0.0, 1.0) end)
    closeBtn:OnClick(function()
        HideScoreboard()
    end)
end

function HideScoreboard()
    if ScorePanel then
        UIManager.DestroyElement(ScorePanel)
        ScorePanel = nil
    end
end
