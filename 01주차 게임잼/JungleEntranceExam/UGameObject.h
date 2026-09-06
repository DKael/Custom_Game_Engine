#pragma once
#include "URenderer.h"
static unsigned int NextID = 0;

// 화면의 경계 위치를 나타내는 변수
inline const float leftBorder = -1.0f;
inline const float rightBorder = 1.0f;
inline const float topBorder = 1.0f;
inline const float bottomBorder = -1.0f;

class UGameObject
{
public:
    UGameObject() : ObjectID(NextID++)
    {
    }

    virtual ~UGameObject() = default;
    // 물리/이동 업데이트
    virtual void Update(float deltaTime) = 0;

    // 렌더링
    virtual void Render(URenderer& renderer) = 0;


    unsigned int GetID() const { return ObjectID; }

private:
    const unsigned int ObjectID;
};
