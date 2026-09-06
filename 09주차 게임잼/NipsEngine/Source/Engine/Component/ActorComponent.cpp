#include "ActorComponent.h"

#include "GameFramework/Actor.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UActorComponent, UObject)
REGISTER_FACTORY(UActorComponent)

void UActorComponent::BeginPlay()
{
    if (bAutoActivate)
    {
        Activate();
    }
}

void UActorComponent::Activate()
{
    bCanEverTick = true;
}

void UActorComponent::Deactivate()
{
    bCanEverTick = false;
}

void UActorComponent::ExecuteTick(float DeltaTime)
{
    if (bCanEverTick == false || bIsActive == false)
    {
        return;
    }

    TickComponent(DeltaTime);
}

void UActorComponent::SetActive(bool bNewActive)
{
    if (bNewActive == bIsActive)
    {
        return;
    }

    bIsActive = bNewActive;

    if (bIsActive)
    {
        Activate();
    }
    else
    {
        Deactivate();
    }
}

void UActorComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    OutProps.push_back({"Enable Tick", EPropertyType::Bool, &bCanEverTick});
    OutProps.push_back({"Editor Only", EPropertyType::Bool, &bIsEditorOnly});
}

void UActorComponent::OnRegister()
{
    if (bRegistered)
    {
        return;
    }

    UWorld* World = nullptr;
    if (!TryGetOwnerWorld(World))
    {
        return;
    }

    RegisterComponentWithWorld(*World);
    bRegistered = true;
}

void UActorComponent::OnUnregister()
{
    if (!bRegistered)
    {
        return;
    }

    UWorld* World = nullptr;
    if (!TryGetOwnerWorld(World))
    {
        return;
    }

    UnregisterComponentFromWorld(*World);
    bRegistered = false;
}

bool UActorComponent::TryGetOwnerWorld(UWorld*& OutWorld) const
{
    OutWorld = nullptr;
    if (Owner == nullptr)
    {
        return false;
    }

    OutWorld = Owner->GetFocusedWorld();
    return OutWorld != nullptr;
}

void UActorComponent::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    Ar << "EnableTick" << bCanEverTick;
    Ar << "EditorOnly" << bIsEditorOnly;
    Ar << "HiddenInEditor" << bHiddenInEditor;
}
