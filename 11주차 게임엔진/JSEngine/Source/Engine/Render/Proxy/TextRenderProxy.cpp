#include "TextRenderProxy.h"
#include "Component/TextRenderComponent.h"

void FTextRenderProxy::CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus)
{
    if (!Context.ShowFlags.bBillboardText)
        return;

    const FFontResource* Font = TextComp->GetFont();
    if (!Font || !Font->IsLoaded())
        return;

    const FString& Text = TextComp->GetText();
    if (Text.empty())
        return;

    FRenderCommand Cmd = {};
    Cmd.Type = ERenderCommandType::Font;
    Cmd.VertexFactoryType = EVertexFactoryType::Text;
    Cmd.SourcePrimitive = TextComp;
    Cmd.PerObjectConstants = FPerObjectConstants{ TextComp->GetWorldMatrix(), TextComp->GetColor() };
    Cmd.Constants.Font.Text = &Text;
    Cmd.Constants.Font.Font = Font;
    Cmd.Constants.Font.Scale = TextComp->GetFontSize();

    RenderBus.AddCommand(ERenderPass::Font, Cmd);
}
