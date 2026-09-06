#include "Animation/AnimNotifyPayloadViews.h"

#include "Animation/AnimNotifyPayloadParser.h"
#include "Animation/AnimNotifySemanticFieldNames.h"

// These builders intentionally stay thin: they interpret semantic fields and
// defaults for one notify shape, but leave raw parsing, validation, and runtime
// execution ownership in their respective layers.
FPlaySfxPayloadView BuildPlaySfxPayloadView(const FAnimNotifyPayloadParser& Payload)
{
    FPlaySfxPayloadView View;
    View.SoundCue = Payload.GetStringAny(
        AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::SoundCue));
    View.SocketName = Payload.GetNameAny(
        AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::SocketName));
    View.VolumeMultiplier = Payload.GetFloatAny(
        AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::VolumeMultiplier),
        1.0f);
    View.bSpatialized = Payload.GetBoolAny(
        AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::Spatialized),
        false);
    return View;
}

FPlayVfxPayloadView BuildPlayVfxPayloadView(const FAnimNotifyPayloadParser& Payload)
{
    FPlayVfxPayloadView View;
    View.Effect = Payload.GetStringAny(
        AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::Effect));
    View.SocketName = Payload.GetNameAny(
        AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::SocketName));
    View.bAttached = Payload.GetBoolAny(
        AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::Attached),
        false);
    View.Scale = Payload.GetFloatAny(
        AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::Scale),
        1.0f);
    return View;
}

FCameraShakePayloadView BuildCameraShakePayloadView(const FAnimNotifyPayloadParser& Payload)
{
    FCameraShakePayloadView View;
    View.Shake = Payload.GetStringAny(
        AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::Shake));
    View.Scale = Payload.GetFloatAny(
        AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::Scale),
        1.0f);
    return View;
}
