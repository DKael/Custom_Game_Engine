#pragma once

#include <functional>

class FGameInputController;

class FPIEController
{
public:
    void Tick(float DeltaTime, FGameInputController& GameInputController);

    void SetEndPIECallback(std::function<void()> Callback) { EndPIECallback = std::move(Callback); }
    void ClearEndPIECallback() { EndPIECallback = nullptr; }

    void Reset();
    bool ShouldBlockGameplayInput() const { return bHostControlReleased; }

private:
    std::function<void()> EndPIECallback;
    bool bHostControlReleased = false;
};
