#include "LightComponent.h"

#include "Object/ObjectFactory.h"

DEFINE_CLASS(ULightComponentBase, USceneComponent)
REGISTER_FACTORY(ULightComponentBase)

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "LightColor", EPropertyType::Color, &LightColor });
    OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity, 0.0f, 20.0f, 0.1f });
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bVisible });
    OutProps.push_back({ "Cast Shadows", EPropertyType::Bool, &bCastShadows });
    OutProps.push_back({ "Debug Line", EPropertyType::Bool, &bDebugDraw });
}

void ULightComponentBase::PostEditProperty(const char* PropertyName)
{
    USceneComponent::PostEditProperty(PropertyName);
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    SerializeLightCommon(Ar);
}

void ULightComponentBase::BeginPlay()
{
    USceneComponent::BeginPlay();
}

void ULightComponentBase::EndPlay()
{
    USceneComponent::EndPlay();
}

void ULightComponentBase::PostDuplicate(UObject* Original)
{
    USceneComponent::PostDuplicate(Original);

    const ULightComponentBase* Orig = Cast<ULightComponentBase>(Original);
    if (!Orig)
    {
        return;
    }

    CopyDuplicateStateFrom(*Orig);
}

DEFINE_CLASS(ULightComponent, ULightComponentBase)
REGISTER_FACTORY(ULightComponent)

ULightComponent::ULightComponent() = default;

void ULightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponentBase::GetEditableProperties(OutProps);

    OutProps.push_back({ "Shadow Resolution Scale", EPropertyType::Float, &ShadowResolutionScale, 0.125f, 4.0f, 0.125f });
    OutProps.push_back({ "Shadow Bias", EPropertyType::Float, &ShadowBias, 0.0f, 1.0f, 0.001f });
    OutProps.push_back({ "Shadow Slope Bias", EPropertyType::Float, &ShadowSlopeBias, 0.0f, 3.0f, 0.01f });
    OutProps.push_back({ "Shadow Sharpen", EPropertyType::Float, &ShadowSharpen, 0.0f, 1.0f, 0.01f });
}

void ULightComponent::Serialize(FArchive& Ar)
{
    ULightComponentBase::Serialize(Ar);
    SerializeShadowSettings(Ar);
}

void ULightComponent::PostDuplicate(UObject* Original)
{
    ULightComponentBase::PostDuplicate(Original);

    const ULightComponent* Orig = Cast<ULightComponent>(Original);
    if (!Orig)
    {
        return;
    }

    CopyDuplicateStateFrom(*Orig);
}

void ULightComponentBase::SerializeLightCommon(FArchive& Ar)
{
    Ar << "LightColor" << LightColor;
    Ar << "Intensity" << Intensity;
    Ar << "Visible" << bVisible;
    Ar << "CastShadows" << bCastShadows;
    Ar << "DebugLine" << bDebugDraw;
}

void ULightComponentBase::CopyDuplicateStateFrom(const ULightComponentBase& Original)
{
    LightColor = Original.LightColor;
    Intensity = Original.Intensity;
    bVisible = Original.bVisible;
    bCastShadows = Original.bCastShadows;
    bDebugDraw = Original.bDebugDraw;
    LightHandle = FLightHandle();
}

void ULightComponentBase::RegisterComponentWithWorld(UWorld& World)
{
    World.RegisterLight(this);
}

void ULightComponentBase::UnregisterComponentFromWorld(UWorld& World)
{
    World.UnregisterLight(this);
}

void ULightComponent::SerializeShadowSettings(FArchive& Ar)
{
    uint32 LightTypeValue = static_cast<uint32>(LightType);
    Ar << "LightType" << LightTypeValue;
    LightType = static_cast<ELightType>(LightTypeValue);

    Ar << "ShadowResolutionScale" << ShadowResolutionScale;
    Ar << "ShadowBias" << ShadowBias;
    Ar << "ShadowSlopeBias" << ShadowSlopeBias;
    Ar << "ShadowSharpen" << ShadowSharpen;
}

void ULightComponent::CopyDuplicateStateFrom(const ULightComponent& Original)
{
    LightType = Original.LightType;
}
