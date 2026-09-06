#include "FireballRenderProxy.h"
#include "Component/FireballComponent.h"

void FFireballRenderProxy::CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus)
{
    FLightData LightData = {};
    LightData.Intensity = FireballComp->GetIntensity();
    LightData.Radius = FireballComp->GetRadius();
    LightData.RadiusFalloff = FireballComp->GetRadiusFallOff();
    LightData.WorldPos = FireballComp->GetWorldLocation();

    FColor Color = FireballComp->GetLinearColor();
    LightData.Color.X = Color.R;
    LightData.Color.Y = Color.G;
    LightData.Color.Z = Color.B;
}
