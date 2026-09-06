#pragma once

#include <cmath>
#include <algorithm>
#include <chrono>

#include "UGameObject.h"

#include "FVector.h"
#include "FColor.h"

inline const float Pi = 3.1415926535f;

// 도형 기본 클래스
class UDiagram : public UGameObject
{
public:
    virtual ~UDiagram() {}


    // 벽 충돌 적용
    virtual void ApplyWallCollision() = 0;

    // 중력 적용
    virtual void ApplyGravity(float deltaTime, const FVector& gravity) = 0;

    // 다른 도형과 충돌 판정
    virtual bool CheckCollision(const UDiagram* Other) = 0;
};