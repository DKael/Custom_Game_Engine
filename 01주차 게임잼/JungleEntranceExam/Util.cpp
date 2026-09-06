#include "Util.h"

int GetRandomSide()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    // 0 또는 1을 균등하게 뽑음
    std::uniform_int_distribution<int> dis(0, 1);

    // 0이면 -1, 1이면 1을 반환
    return (dis(gen) == 0) ? -1 : 1;
}