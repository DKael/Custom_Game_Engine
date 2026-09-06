#pragma once
#include "Types.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include <vector>
#include <string>

class UGameObject;
class URenderer;

class UScene
{
public:
    UScene() = default;
    virtual ~UScene() = default;

    UGameObject* SearchObject(unsigned int ID);
    virtual void Update(float delta);
    virtual void Render(URenderer renderer) = 0;
    virtual void UIRender()= 0;

    void SetActive(bool doActive) { bisActive = doActive; }

    virtual void Init() = 0;
    virtual void Release() = 0;
    virtual void AddObject(UGameObject* Object) { UGameObjectList.push_back(Object); }

    ESceneType GetSceneType() { return SceneType; }

private:
    void RemoveObject(UGameObject* Object);


protected:
    //std::string SceneName;
    std::vector<UGameObject*> UGameObjectList;
    bool bisActive{ false };
    ESceneType SceneType{ ESceneType ::None};
};