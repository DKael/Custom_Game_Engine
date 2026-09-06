#pragma once
#include "UScene.h"

class UUIButton;
class UGameObject;

class UTitleScene :
    public UScene
{

public:
    UTitleScene() = default;
    virtual ~UTitleScene() { Release(); }

    void Init() override;
    void Update(float delta) override;
    void Render(URenderer render) override;
    void Release() override;

    void GameStart();
    void GameEnd();

private:
    UGameObject* TitleLogo = nullptr;
    UUIButton* StartButton = nullptr;
    UUIButton* EndButton = nullptr;
    UUIButton* CreditButton = nullptr;

    void UIRender() override;
};

