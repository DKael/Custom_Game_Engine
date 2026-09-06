#include "SkeletalMesh.h"

#include "Animation/AnimData/AnimSequence.h"
#include "Asset/Skeleton.h"
#include "Core/Logging/Log.h"
#include "Engine/Geometry/Transform.h"

namespace
{
    void DestroyOwnedSkeleton(USkeleton*& Skeleton)
    {
        if (!Skeleton)
        {
            return;
        }

        // Skeletal meshes are the primary owner of externally loaded skeleton
        // objects, but other runtime/editor objects may still keep raw
        // references. If the skeleton was already destroyed during an earlier
        // cleanup step, treat the pointer as stale and just clear it.
        if (UObjectManager::Get().ContainsObject(Skeleton))
        {
            UObjectManager::Get().DestroyObject(Skeleton);
        }
        Skeleton = nullptr;
    }
}

const TArray<FBoneInfo>& FSkeletalMesh::GetBones() const
{
    static const TArray<FBoneInfo> Empty = {};
    return Skeleton ? Skeleton->GetBones() : Empty;
}

TArray<FBoneInfo>* FSkeletalMesh::GetMutableBones()
{
    if (!Skeleton || !Skeleton->GetSkeletonData())
    {
        return nullptr;
    }

    return &Skeleton->GetSkeletonData()->Bones;
}

bool FSkeletalMesh::HasValidSkeletonData() const
{
    return Skeleton != nullptr && Skeleton->HasValidSkeletonData();
}

FMatrix FSkeletalMeshSocket::GetRelativeTransform() const
{
    // row-vector 규약: v · S · R · T  (FTransform::ToMatrixWithScale가 동일 합성)
    return FTransform(RelativeRotation, RelativeLocation, RelativeScale).ToMatrixWithScale();
}

USkeletalMesh::~USkeletalMesh()
{
    if (MeshData != nullptr)
    {
        DestroyOwnedSkeleton(MeshData->Skeleton);
        delete MeshData;
        MeshData = nullptr;
    }
}

void USkeletalMesh::SetMeshData(FSkeletalMesh* InMeshData)
{
    if (MeshData == InMeshData)
    {
        return;
    }

    if (MeshData)
    {
        DestroyOwnedSkeleton(MeshData->Skeleton);
    }
    delete MeshData;
    MeshData = InMeshData;

    RebuildLocalBoundsFromMeshData();
}

FSkeletalMesh* USkeletalMesh::GetMeshData()
{
    return MeshData;
}

const FSkeletalMesh* USkeletalMesh::GetMeshData() const
{
    return MeshData;
}

const FString& USkeletalMesh::GetAssetPathFileName() const
{
    static FString Empty = {};
    return MeshData ? MeshData->PathFileName : Empty;
}

const TArray<FSkeletalMeshVertex>& USkeletalMesh::GetVertices() const
{
    static const TArray<FSkeletalMeshVertex> Empty = {};
    return MeshData ? MeshData->Vertices : Empty;
}

const TArray<uint32>& USkeletalMesh::GetIndices() const
{
    static const TArray<uint32> Empty = {};
    return MeshData ? MeshData->Indices : Empty;
}

const TArray<FBoneInfo>& USkeletalMesh::GetBones() const
{
    static const TArray<FBoneInfo> Empty = {};
    return MeshData ? MeshData->GetBones() : Empty;
}

const TArray<UAnimSequence*>& USkeletalMesh::GetAnimationSequences() const
{
    static const TArray<UAnimSequence*> Empty = {};
    return Empty;
}

void USkeletalMesh::SetSkeleton(USkeleton* InSkeleton)
{
    if (!MeshData)
    {
        return;
    }

    if (MeshData->Skeleton == InSkeleton)
    {
        return;
    }

    DestroyOwnedSkeleton(MeshData->Skeleton);
    MeshData->Skeleton = InSkeleton;
}

USkeleton* USkeletalMesh::GetSkeleton() const
{
    return MeshData ? MeshData->Skeleton : nullptr;
}

const FString& USkeletalMesh::GetSkeletonAssetPath() const
{
    static const FString Empty = {};
    return MeshData ? MeshData->SkeletonAssetPath : Empty;
}

const FBoneInfo* USkeletalMesh::GetBoneInfo(int32 BoneIndex) const
{
    if (!MeshData)
    {
        return nullptr;
    }

    const TArray<FBoneInfo>& Bones = MeshData->GetBones();
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return nullptr;
    }

    return &Bones[BoneIndex];
}

const FMatrix& USkeletalMesh::GetLocalBindTransform(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    const FBoneInfo* Bone = GetBoneInfo(BoneIndex);
    return Bone ? Bone->LocalBindTransform : Identity;
}

const FMatrix& USkeletalMesh::GetGlobalBindTransform(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    const FBoneInfo* Bone = GetBoneInfo(BoneIndex);
    return Bone ? Bone->GlobalBindTransform : Identity;
}

const FMatrix& USkeletalMesh::GetInverseBindPose(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    const FBoneInfo* Bone = GetBoneInfo(BoneIndex);
    return Bone ? Bone->InverseBindPose : Identity;
}

const TArray<FStaticMeshSection>& USkeletalMesh::GetSections() const
{
    static const TArray<FStaticMeshSection> Empty = {};
    return MeshData ? MeshData->Sections : Empty;
}

const TArray<FStaticMeshMaterialSlot>& USkeletalMesh::GetMaterialSlots() const
{
    static const TArray<FStaticMeshMaterialSlot> Empty = {};
    return MeshData ? MeshData->MaterialSlots : Empty;
}

const TArray<FSkeletalMeshSocket>& USkeletalMesh::GetSockets() const
{
    static const TArray<FSkeletalMeshSocket> Empty = {};
    return MeshData ? MeshData->Sockets : Empty;
}

const FSkeletalMeshSocket* USkeletalMesh::FindSocket(const FName& Name) const
{
    if (!MeshData || !Name.IsValid())
    {
        return nullptr;
    }

    for (const FSkeletalMeshSocket& Socket : MeshData->Sockets)
    {
        if (Socket.Name == Name)
        {
            return &Socket;
        }
    }

    return nullptr;
}

bool USkeletalMesh::HasSocket(const FName& Name) const
{
    return FindSocket(Name) != nullptr;
}

const FAABB& USkeletalMesh::GetLocalBounds() const
{
    static const FAABB Empty = {};
    return MeshData ? MeshData->LocalBounds : Empty;
}

bool USkeletalMesh::HasValidMeshData() const
{
    return MeshData != nullptr
        && !MeshData->Vertices.empty()
        && !MeshData->Indices.empty()
        && MeshData->HasValidSkeletonData();
}

void USkeletalMesh::RebuildLocalBoundsFromMeshData()
{
    if (!MeshData)
    {
        return;
    }

    MeshData->LocalBounds.Reset();

    for (const FSkeletalMeshVertex& Vertex : MeshData->Vertices)
    {
        MeshData->LocalBounds.Expand(Vertex.Position);
    }
}
