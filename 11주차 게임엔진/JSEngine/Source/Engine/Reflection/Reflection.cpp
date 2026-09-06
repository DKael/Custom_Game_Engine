#include "Reflection/Reflection.h"

#include "Component/ActorComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Object/Object.h"
#include "Serialization/Archive.h"

void UStruct::AppendProperties(void* StructValue, TArray<FPropertyDescriptor>& OutProps) const
{
    if (!StructValue || !Properties)
    {
        return;
    }

    for (uint32 Index = 0; Index < PropertyCount; ++Index)
    {
        const FProperty* Property = Properties[Index];
        if (Property)
        {
            Property->AppendEditorDescriptor(StructValue, OutProps);
        }
    }
}

void UStruct::SerializeProperties(FArchive& Ar, UObject* OwnerObject, void* StructValue) const
{
    if (!StructValue || !Properties)
    {
        return;
    }

    for (uint32 Index = 0; Index < PropertyCount; ++Index)
    {
        const FProperty* Property = Properties[Index];
        if (!Property || !Property->ShouldSerialize())
        {
            continue;
        }

        const char* Key = Property->GetSerializeKey();
        if (!Key || (Property->ShouldCheckSerializeKey() && Ar.IsLoading() && !Ar.HasKey(Key)))
        {
            continue;
        }

        Property->SerializeItem(Ar, OwnerObject, StructValue);
    }
}

void UClass::AppendProperties(void* ObjectValue, TArray<FPropertyDescriptor>& OutProps) const
{
    if (SuperClass)
    {
        SuperClass->AppendProperties(ObjectValue, OutProps);
    }

    UStruct::AppendProperties(ObjectValue, OutProps);
}

void UClass::SerializeProperties(FArchive& Ar, UObject* ObjectValue) const
{
    if (SuperClass)
    {
        SuperClass->SerializeProperties(Ar, ObjectValue);
    }

    UStruct::SerializeProperties(Ar, ObjectValue, ObjectValue);
}

void FProperty::AppendEditorDescriptor(void* Container, TArray<FPropertyDescriptor>& OutProps) const
{
    if (!Container || !GetName())
    {
        return;
    }

    OutProps.push_back({
        GetName(),
        GetType(),
        ContainerPtrToValuePtr(Container),
        GetMin(),
        GetMax(),
        GetSpeed(),
        nullptr,
        0,
        nullptr,
        GetDescriptorUsageFlags(),
        GetObjectType(),
        GetDisplayName()
    });
}

void FBoolProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    Ar << GetSerializeKey() << *static_cast<bool*>(ContainerPtrToValuePtr(Container));
}

void FIntProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    Ar << GetSerializeKey() << *static_cast<int32*>(ContainerPtrToValuePtr(Container));
}

void FFloatProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    Ar << GetSerializeKey() << *static_cast<float*>(ContainerPtrToValuePtr(Container));
}

void FVectorProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    Ar << GetSerializeKey() << *static_cast<FVector*>(ContainerPtrToValuePtr(Container));
}

void FVector4Property::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    Ar << GetSerializeKey() << *static_cast<FVector4*>(ContainerPtrToValuePtr(Container));
}

void FStringProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    Ar << GetSerializeKey() << *static_cast<FString*>(ContainerPtrToValuePtr(Container));
}

void FNameProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    Ar << GetSerializeKey() << *static_cast<FName*>(ContainerPtrToValuePtr(Container));
}

void FColorProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    Ar << GetSerializeKey() << *static_cast<FColor*>(ContainerPtrToValuePtr(Container));
}

void FObjectProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    UObject*& Ref = *static_cast<UObject**>(ContainerPtrToValuePtr(Container));
    uint32 RefUUID = Ref ? Ref->GetUUID() : 0;
    Ar << GetSerializeKey() << RefUUID;
    if (Ar.IsLoading())
    {
        Ref = nullptr;
        for (UObject* ObjectCandidate : GUObjectArray)
        {
            if (ObjectCandidate && ObjectCandidate->GetUUID() == RefUUID)
            {
                Ref = ObjectCandidate;
                break;
            }
        }
    }
}

void FSceneComponentProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    USceneComponent*& Ref = *static_cast<USceneComponent**>(ContainerPtrToValuePtr(Container));
    uint32 RefUUID = Ref ? Ref->GetUUID() : 0;
    Ar << GetSerializeKey() << RefUUID;
    if (Ar.IsLoading())
    {
        Ref = nullptr;
        UActorComponent* OwnerComponent = Cast<UActorComponent>(OwnerObject);
        AActor* Owner = OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
        if (!Owner)
        {
            return;
        }

        for (UActorComponent* Component : Owner->GetComponents())
        {
            USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
            if (SceneComponent && SceneComponent->GetUUID() == RefUUID)
            {
                Ref = SceneComponent;
                break;
            }
        }
    }
}

void FStructProperty::AppendEditorDescriptor(void* Container, TArray<FPropertyDescriptor>& OutProps) const
{
    if (!Struct)
    {
        return;
    }

    Struct->AppendProperties(ContainerPtrToValuePtr(Container), OutProps);
}

void FStructProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    if (Struct)
    {
        Struct->SerializeProperties(Ar, OwnerObject, ContainerPtrToValuePtr(Container));
    }
}

void FEnumProperty::AppendEditorDescriptor(void* Container, TArray<FPropertyDescriptor>& OutProps) const
{
    if (!Container || !GetName())
    {
        return;
    }

    OutProps.push_back({
        GetName(),
        GetType(),
        ContainerPtrToValuePtr(Container),
        GetMin(),
        GetMax(),
        GetSpeed(),
        Enum ? Enum->GetNames() : nullptr,
        Enum ? Enum->GetValueCount() : 0,
        nullptr,
        GetDescriptorUsageFlags(),
        nullptr,
        GetDisplayName()
    });
}

void FEnumProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    Ar << GetSerializeKey() << *static_cast<int32*>(ContainerPtrToValuePtr(Container));
}

void FArrayProperty::SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const
{
    (void)OwnerObject;
    Ar << GetSerializeKey() << *static_cast<TArray<FVector>*>(ContainerPtrToValuePtr(Container));
}
