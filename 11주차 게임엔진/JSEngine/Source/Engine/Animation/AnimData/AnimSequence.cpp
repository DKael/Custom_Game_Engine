#include "Animation/AnimData/AnimSequence.h"

#include "Animation/Compression/AclCompression.h"

UAnimSequence::~UAnimSequence()
{
    delete DataModel;
    DataModel = nullptr;

    ClearAclRuntimeData();
}

bool UAnimSequence::HasAclRuntimeData() const
{
    return AclRuntimeData && AclRuntimeData->IsValid();
}

const FAclAnimSequenceRuntimeData* UAnimSequence::GetAclRuntimeData() const
{
    return AclRuntimeData;
}

FAclAnimSequenceRuntimeData* UAnimSequence::GetMutableAclRuntimeData()
{
    if (!AclRuntimeData)
    {
        AclRuntimeData = new FAclAnimSequenceRuntimeData();
    }

    return AclRuntimeData;
}

void UAnimSequence::SetAclRuntimeData(FAclAnimSequenceRuntimeData* InAclRuntimeData)
{
    if (AclRuntimeData == InAclRuntimeData)
    {
        return;
    }

    ClearAclRuntimeData();
    AclRuntimeData = InAclRuntimeData;
}

void UAnimSequence::ClearAclRuntimeData()
{
    delete AclRuntimeData;
    AclRuntimeData = nullptr;
}
