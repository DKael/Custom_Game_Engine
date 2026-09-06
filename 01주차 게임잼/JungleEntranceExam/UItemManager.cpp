#include "UItemManager.h"
#include "UBar.h"

UItemManager& UItemManager::Get()
{
    static UItemManager Instance;
    return Instance;
}

UItemManager::UItemManager()
{
}

UItemManager::~UItemManager()
{
    Shutdown();
}

void UItemManager::Initialize()
{
}

void UItemManager::Shutdown()
{
    Clear();
}

void UItemManager::Update(float DeltaTime)
{
    for (int i = 0; i < ItemCount; ++i)
    {
        if (ItemList[i] != nullptr)
        {
            ItemList[i]->Update(DeltaTime);
        }
    }

    RemoveDeadItems();
}

void UItemManager::Render(URenderer& renderer)
{
    for (int i = 0; i < ItemCount; ++i)
    {
        if (ItemList[i] != nullptr)
        {
            ItemList[i]->Render(renderer);
        }
    }
}

void UItemManager::Clear()
{
    for (int i = 0; i < ItemCount; ++i)
    {
        delete ItemList[i];
        ItemList[i] = nullptr;
    }

    delete[] ItemList;
    ItemList = nullptr;

    ItemCount = 0;
    Capacity = 0;
}

void UItemManager::SpawnItem(const FItemDesc& ItemDesc, const FVector& SpawnPosition, const FVector& Direction)
{
    UItem* NewItem = new UItem(SpawnPosition, Direction, 0.1f, 0.035f, ItemDesc);
    AddItem(NewItem);
}

void UItemManager::CheckCollision(UDiagram* Paddle)
{
    for (int i = 0; i < ItemCount; ++i)
    {
        ItemList[i]->CheckCollision(Paddle);
    }

    for (int i = 0; i < ItemCount; ++i)
    {
        UItem* Item = ItemList[i];
        if (Item == nullptr || !Item->IsAlive())
            continue;

        if (Item->CheckCollision(Paddle))
        {
            UBar* PlayerBar = dynamic_cast<UBar*>(Paddle);
            if (PlayerBar)
            {
                Item->ApplyTo(PlayerBar);
            }
        }
    }
}

void UItemManager::RemoveDeadItems()
{
    for (int i = ItemCount - 1; i >= 0; --i)
    {
        if (ItemList[i] == nullptr || !ItemList[i]->IsAlive())
        {
            RemoveItemAt(i);
        }
    }
}

int UItemManager::GetItemCount() const
{
    return ItemCount;
}

void UItemManager::Reserve(int NewCapacity)
{
    if (NewCapacity <= Capacity)
        return;

    UItem** NewList = new UItem * [NewCapacity];

    for (int i = 0; i < ItemCount; ++i)
    {
        NewList[i] = ItemList[i];
    }

    for (int i = ItemCount; i < NewCapacity; ++i)
    {
        NewList[i] = nullptr;
    }

    delete[] ItemList;
    ItemList = NewList;
    Capacity = NewCapacity;
}

void UItemManager::AddItem(UItem* NewItem)
{
    if (ItemCount >= Capacity)
    {
        const int NewCapacity = (Capacity == 0) ? 4 : Capacity * 2;
        Reserve(NewCapacity);
    }

    ItemList[ItemCount] = NewItem;
    ++ItemCount;
}

void UItemManager::RemoveItemAt(int Index)
{
    if (Index < 0 || Index >= ItemCount)
        return;

    delete ItemList[Index];
    ItemList[Index] = nullptr;

    for (int i = Index; i < ItemCount - 1; ++i)
    {
        ItemList[i] = ItemList[i + 1];
    }

    ItemList[ItemCount - 1] = nullptr;
    --ItemCount;
}
