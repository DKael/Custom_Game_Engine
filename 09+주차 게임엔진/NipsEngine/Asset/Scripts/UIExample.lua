-- UI 연동 예시 스크립트
-- UIManager를 통해 HUD 요소를 생성하고 매 프레임 업데이트하는 패턴

local healthBar = nil
local scoreText = nil
local Score = 0

function OnStart(self)
    -- 체력바: 화면 왼쪽 상단 (x=20, y=20, w=200, h=20)
    healthBar = UIManager.CreateProgressBar(20, 20, 200, 20)
    if healthBar then
        healthBar:SetValue(1.0)
        healthBar:SetFillColor(0.2, 0.8, 0.2, 1.0)  -- 초록
        healthBar:SetBgColor(0.2, 0.2, 0.2, 0.8)
    end

    -- 점수 텍스트: 화면 오른쪽 상단
    scoreText = UIManager.CreateText(900, 20, 300, 30, "Score: 0")
    if scoreText then
        scoreText:SetColor(1.0, 1.0, 0.0, 1.0)  -- 노란색
        scoreText:SetFontSize(20.0)
    end

    Log("UIExample: HUD created for " .. self:GetName())
end

function OnUpdate(self, deltaTime)
    Score = Score + deltaTime * 10
    if scoreText then
        scoreText:SetText("Score: " .. math.floor(Score))
    end
end

function OnDestroy(self)
    if healthBar then
        UIManager.DestroyElement(healthBar)
        healthBar = nil
    end
    if scoreText then
        UIManager.DestroyElement(scoreText)
        scoreText = nil
    end
end
