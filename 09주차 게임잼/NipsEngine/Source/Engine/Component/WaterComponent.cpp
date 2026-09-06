#include "WaterComponent.h"

#include "Object/ObjectFactory.h"
#include "Render/Scene/RenderCommand.h"

DEFINE_CLASS(UWaterComponent, UActorComponent)
REGISTER_FACTORY(UWaterComponent)

void UWaterComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);

    Ar << "NormalStrength" << SurfaceProfile.NormalStrength;
    Ar << "Alpha" << SurfaceProfile.Alpha;
    Ar << "ColorVariationStrength" << SurfaceProfile.ColorVariationStrength;

    Ar << "NormalTilingAX" << SurfaceProfile.NormalTilingAX;
    Ar << "NormalTilingAY" << SurfaceProfile.NormalTilingAY;
    Ar << "NormalScrollSpeedAX" << SurfaceProfile.NormalScrollSpeedAX;
    Ar << "NormalScrollSpeedAY" << SurfaceProfile.NormalScrollSpeedAY;

    Ar << "NormalTilingBX" << SurfaceProfile.NormalTilingBX;
    Ar << "NormalTilingBY" << SurfaceProfile.NormalTilingBY;
    Ar << "NormalScrollSpeedBX" << SurfaceProfile.NormalScrollSpeedBX;
    Ar << "NormalScrollSpeedBY" << SurfaceProfile.NormalScrollSpeedBY;
    Ar << "WorldUVScaleX" << SurfaceProfile.WorldUVScaleX;
    Ar << "WorldUVScaleY" << SurfaceProfile.WorldUVScaleY;
    Ar << "WorldUVBlendFactor" << SurfaceProfile.WorldUVBlendFactor;

    Ar << "BaseColor" << SurfaceProfile.BaseColor;
    Ar << "WaterSpecularPower" << SurfaceProfile.WaterSpecularPower;
    Ar << "WaterSpecularIntensity" << SurfaceProfile.WaterSpecularIntensity;
    Ar << "WaterFresnelPower" << SurfaceProfile.WaterFresnelPower;
    Ar << "WaterFresnelIntensity" << SurfaceProfile.WaterFresnelIntensity;
    Ar << "WaterLightContributionScale" << SurfaceProfile.WaterLightContributionScale;
    Ar << "HorizonFadeStart" << SurfaceProfile.HorizonFadeStart;
    Ar << "HorizonFadeEnd" << SurfaceProfile.HorizonFadeEnd;
    Ar << "NdotLFadeWidth" << SurfaceProfile.NdotLFadeWidth;
    Ar << "EnableWaterSpecular" << SurfaceProfile.bEnableWaterSpecular;
}

void UWaterComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "NormalStrength", EPropertyType::Float, &SurfaceProfile.NormalStrength, 0.0f, 2.0f, 0.01f });
    OutProps.push_back({ "Alpha", EPropertyType::Float, &SurfaceProfile.Alpha, 0.0f, 1.0f, 0.01f });
    OutProps.push_back({ "ColorVariationStrength", EPropertyType::Float, &SurfaceProfile.ColorVariationStrength, 0.0f, 1.0f, 0.01f });

    OutProps.push_back({ "NormalTilingAX", EPropertyType::Float, &SurfaceProfile.NormalTilingAX, 0.01f, 32.0f, 0.01f });
    OutProps.push_back({ "NormalTilingAY", EPropertyType::Float, &SurfaceProfile.NormalTilingAY, 0.01f, 32.0f, 0.01f });
    OutProps.push_back({ "NormalScrollSpeedAX", EPropertyType::Float, &SurfaceProfile.NormalScrollSpeedAX, -5.0f, 5.0f, 0.001f });
    OutProps.push_back({ "NormalScrollSpeedAY", EPropertyType::Float, &SurfaceProfile.NormalScrollSpeedAY, -5.0f, 5.0f, 0.001f });

    OutProps.push_back({ "NormalTilingBX", EPropertyType::Float, &SurfaceProfile.NormalTilingBX, 0.01f, 32.0f, 0.01f });
    OutProps.push_back({ "NormalTilingBY", EPropertyType::Float, &SurfaceProfile.NormalTilingBY, 0.01f, 32.0f, 0.01f });
    OutProps.push_back({ "NormalScrollSpeedBX", EPropertyType::Float, &SurfaceProfile.NormalScrollSpeedBX, -5.0f, 5.0f, 0.001f });
    OutProps.push_back({ "NormalScrollSpeedBY", EPropertyType::Float, &SurfaceProfile.NormalScrollSpeedBY, -5.0f, 5.0f, 0.001f });
    OutProps.push_back({ "WorldUVScaleX", EPropertyType::Float, &SurfaceProfile.WorldUVScaleX, 0.0001f, 1.0f, 0.0005f });
    OutProps.push_back({ "WorldUVScaleY", EPropertyType::Float, &SurfaceProfile.WorldUVScaleY, 0.0001f, 1.0f, 0.0005f });
    OutProps.push_back({ "WorldUVBlendFactor", EPropertyType::Float, &SurfaceProfile.WorldUVBlendFactor, 0.0f, 1.0f, 0.01f });

    OutProps.push_back({ "BaseColor", EPropertyType::Color, &SurfaceProfile.BaseColor });
    OutProps.push_back({ "WaterSpecularPower", EPropertyType::Float, &SurfaceProfile.WaterSpecularPower, 1.0f, 512.0f, 1.0f });
    OutProps.push_back({ "WaterSpecularIntensity", EPropertyType::Float, &SurfaceProfile.WaterSpecularIntensity, 0.0f, 10.0f, 0.01f });
    OutProps.push_back({ "WaterFresnelPower", EPropertyType::Float, &SurfaceProfile.WaterFresnelPower, 1.0f, 16.0f, 0.1f });
    OutProps.push_back({ "WaterFresnelIntensity", EPropertyType::Float, &SurfaceProfile.WaterFresnelIntensity, 0.0f, 10.0f, 0.01f });
    OutProps.push_back({ "WaterLightContributionScale", EPropertyType::Float, &SurfaceProfile.WaterLightContributionScale, 0.0f, 10.0f, 0.01f });
    OutProps.push_back({ "HorizonFadeStart", EPropertyType::Float, &SurfaceProfile.HorizonFadeStart, -1.0f, 1.0f, 0.01f });
    OutProps.push_back({ "HorizonFadeEnd", EPropertyType::Float, &SurfaceProfile.HorizonFadeEnd, -1.0f, 1.0f, 0.01f });
    OutProps.push_back({ "NdotLFadeWidth", EPropertyType::Float, &SurfaceProfile.NdotLFadeWidth, 0.001f, 1.0f, 0.005f });
    OutProps.push_back({ "EnableWaterSpecular", EPropertyType::Bool, &SurfaceProfile.bEnableWaterSpecular });
}

void UWaterComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);
    (void)PropertyName;
}

void UWaterComponent::FillWaterUniformData(FWaterUniformData& OutData, float TimeSeconds, uint32 LocalLightCount) const
{
    OutData.Time = TimeSeconds;
    OutData.NormalStrength = SurfaceProfile.NormalStrength;
    OutData.Alpha = SurfaceProfile.Alpha;
    OutData.WaterSpecularPower = SurfaceProfile.WaterSpecularPower;

    OutData.NormalTilingA = FVector2(SurfaceProfile.NormalTilingAX, SurfaceProfile.NormalTilingAY);
    OutData.NormalScrollSpeedA = FVector2(SurfaceProfile.NormalScrollSpeedAX, SurfaceProfile.NormalScrollSpeedAY);
    OutData.NormalTilingB = FVector2(SurfaceProfile.NormalTilingBX, SurfaceProfile.NormalTilingBY);
    OutData.NormalScrollSpeedB = FVector2(SurfaceProfile.NormalScrollSpeedBX, SurfaceProfile.NormalScrollSpeedBY);
    OutData.WorldUVScaleX = SurfaceProfile.WorldUVScaleX;
    OutData.WorldUVScaleY = SurfaceProfile.WorldUVScaleY;
    OutData.WorldUVBlendFactor = SurfaceProfile.WorldUVBlendFactor;

    OutData.BaseColor = FVector(SurfaceProfile.BaseColor.R, SurfaceProfile.BaseColor.G, SurfaceProfile.BaseColor.B);
    OutData.WaterSpecularIntensity = SurfaceProfile.WaterSpecularIntensity;
    OutData.ColorVariationStrength = SurfaceProfile.ColorVariationStrength;
    OutData.WaterFresnelPower = SurfaceProfile.WaterFresnelPower;
    OutData.WaterFresnelIntensity = SurfaceProfile.WaterFresnelIntensity;
    OutData.WaterLightContributionScale = SurfaceProfile.WaterLightContributionScale;
    OutData.HorizonFadeStart = SurfaceProfile.HorizonFadeStart;
    OutData.HorizonFadeEnd = SurfaceProfile.HorizonFadeEnd;
    OutData.NdotLFadeWidth = SurfaceProfile.NdotLFadeWidth;
    OutData.bEnableWaterSpecular = SurfaceProfile.bEnableWaterSpecular ? 1u : 0u;
    OutData.WaterLocalLightCount = LocalLightCount;
}

void UWaterComponent::ApplyWaterSurfaceProfile(const FWaterSurfaceProfile& InProfile)
{
    SurfaceProfile = InProfile;
}
