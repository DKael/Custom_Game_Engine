#include "HeightFogRenderProxy.h"
#include "Component/HeightFogComponent.h"

void FHeightFogRenderProxy::CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus)
{
    if (!Context.ShowFlags.bFog)
        return;

    FRenderCommand Cmd = {};
    Cmd.Type = ERenderCommandType::Primitive;
    Cmd.VertexFactoryType = EVertexFactoryType::Primitive;
    Cmd.Constants.Fog.FogDensity = HeightFogComp->GetFogDensity();
    Cmd.Constants.Fog.FogColor = HeightFogComp->GetFogInscatteringColor();
    Cmd.Constants.Fog.HeightFalloff = HeightFogComp->GetHeightFalloff();
    Cmd.Constants.Fog.FogHeight = HeightFogComp->GetFogHeight();
    Cmd.Constants.Fog.FogStartDistance = HeightFogComp->GetFogStartDistance();
    Cmd.Constants.Fog.FogMaxOpacity = HeightFogComp->GetFogMaxOpacity();
    Cmd.Constants.Fog.FogCutoffDistance = HeightFogComp->GetFogCutoffDistance();

    RenderBus.AddCommand(ERenderPass::Fog, Cmd);
}
