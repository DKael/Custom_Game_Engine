#pragma once

#include <random>

template<typename T>
float GetRandomFloat(T min, T max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<T> dis(min, max);

    return dis(gen);
}

int GetRandomSide();