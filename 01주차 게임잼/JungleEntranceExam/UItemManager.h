#pragma once

#include "UItem.h"
#include "ItemEffectReceiver.h"

class UItem;

class UItemManager
{
public:
    static UItemManager& Get();

private:
    UItemManager();
    ~UItemManager();

    UItemManager(const UItemManager&) = delete;
    UItemManager& operator=(const UItemManager&) = delete;

public:
    void Initialize();
    void Shutdown();

    void Update(float DeltaTime);
    void Render(URenderer& renderer);
    void Clear();

public:
    void SpawnItem(const FItemDesc& ItemDesc, const FVector& SpawnPosition, const FVector& Direction);
    void CheckCollision(UDiagram* Paddle);
    void RemoveDeadItems();

    int GetItemCount() const;

private:
    void Reserve(int NewCapacity);
    void AddItem(UItem* NewItem);
    void RemoveItemAt(int Index);

    UItem** ItemList = nullptr;
    int ItemCount = 0;
    int Capacity = 0;
};