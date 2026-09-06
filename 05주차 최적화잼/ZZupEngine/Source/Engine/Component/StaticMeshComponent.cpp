#include "StaticMeshComponent.h"

#include <cfloat>
#include <cstring>

#include "Core/ResourceManager.h"

DEFINE_CLASS(UStaticMeshComponent, UMeshComponent)
REGISTER_FACTORY(UStaticMeshComponent)

UStaticMeshComponent::UStaticMeshComponent()
{
	//	기본 도형은 Cube로 설정
	SetStaticMesh(FResourceManager::Get().LoadStaticMesh("Asset/Mesh/Dice/Dice.obj"));
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	if (StaticMeshAsset == InStaticMesh)
	{
		return;
	}

	StaticMeshAsset = InStaticMesh;
	OverrideMaterial.clear();

	if (StaticMeshAsset != nullptr)
	{
		StaticMeshAssetPath = StaticMeshAsset->GetAssetPathFileName();

		const auto& Slots    = StaticMeshAsset->GetMaterialSlots();
		const auto& Sections = StaticMeshAsset->GetSections();
		OverrideMaterial.reserve(Sections.size());

		for (int32 i = 0; i < static_cast<int32>(Sections.size()); ++i)
		{
			OverrideMaterial.push_back(Slots[Sections[i].MaterialSlotIndex].MaterialData);
		}
	}
	else
	{
		StaticMeshAssetPath.clear();
	}

	MarkBoundsDirty();
	MarkRenderStateDirty();
}

UStaticMesh* UStaticMeshComponent::GetStaticMesh() const
{
	return StaticMeshAsset;
}

bool UStaticMeshComponent::HasValidMesh() const
{
	return StaticMeshAsset != nullptr && StaticMeshAsset->HasValidMeshData();
}


void UStaticMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMeshComponent::GetEditableProperties(OutProps);
	OutProps.push_back({"StaticMesh", EPropertyType::String, &StaticMeshAssetPath});
}

void UStaticMeshComponent::PostEditProperty(const char* PropertyName)
{
	UMeshComponent::PostEditProperty(PropertyName);
	
	//	추후에 FNAme으로 바꿔도 될 듯 싶긴한데 보류
	if (std::strcmp(PropertyName, "StaticMesh") != 0)
	{
		return;
	}
	
	if (StaticMeshAssetPath.empty())
	{
		SetStaticMesh(nullptr);
		return;
	}
	
	UStaticMesh * Mesh = FResourceManager::Get().LoadStaticMesh(StaticMeshAssetPath);
	
	SetStaticMesh(Mesh);
}

void UStaticMeshComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();

	if (!HasValidMesh())
	{
		bBoundsDirty = false;
		return;
	}

	const FAABB& LocalBounds = StaticMeshAsset->GetLocalBounds();
	if (!LocalBounds.IsValid())
	{
		bBoundsDirty = false;
		return;
	}

	const FVector LocalCorners[8] =
	{
		FVector(LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Min.Z),
		FVector(LocalBounds.Max.X, LocalBounds.Min.Y, LocalBounds.Min.Z),
		FVector(LocalBounds.Min.X, LocalBounds.Max.Y, LocalBounds.Min.Z),
		FVector(LocalBounds.Max.X, LocalBounds.Max.Y, LocalBounds.Min.Z),
		FVector(LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Max.Z),
		FVector(LocalBounds.Max.X, LocalBounds.Min.Y, LocalBounds.Max.Z),
		FVector(LocalBounds.Min.X, LocalBounds.Max.Y, LocalBounds.Max.Z),
		FVector(LocalBounds.Max.X, LocalBounds.Max.Y, LocalBounds.Max.Z)
	};

	const FMatrix& WorldMatrix = GetWorldMatrix();

	for (const FVector& Corner : LocalCorners)
	{
		const FVector WorldPos = WorldMatrix.TransformPosition(Corner);
		WorldAABB.Expand(WorldPos);
	}

	bBoundsDirty = false;
}

//	Ray를 Local로 바꿔서 확인 
//	모든 Mesh를 World로 바꾸는 것보다 훨씬 빠름
bool UStaticMeshComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!HasValidMesh())
	{
		return false;
	}

	EnsureBoundsUpdated();

	float BoxT = 0.0f;
	if (!WorldAABB.IntersectRay(Ray, BoxT))
	{
		return false;
	}

	const TArray<FNormalVertex>& Vertices = StaticMeshAsset->GetVertices();
	const TArray<uint32>& Indices = StaticMeshAsset->GetIndices();
	const FBVH& BVH = StaticMeshAsset->GetMeshBVH();

	if (Vertices.empty() || Indices.empty())
	{
		return false;
	}

	const FMatrix InvWorld = GetWorldMatrix().GetInverse();
	FRay LocalRay = Ray;
	LocalRay.Origin = InvWorld.TransformPosition(LocalRay.Origin);

	//K-D Tree
	//return GetStaticMesh()->RaycastMesh(LocalRay, OutHitResult.Distance);

	// Direction 변경 시 SetDirection()을 통해 InvD도 함께 갱신
	LocalRay.SetDirection(InvWorld.TransformVector(LocalRay.Direction).GetSafeNormal());
	
	struct FEntry
	{
		int32 NodeIndex;
		float tEnter; // 충돌에 걸리는 시간
	};
	
    TArray<FEntry> Stack;
    Stack.reserve(64);

	float   ClosestT = FLT_MAX;
    int32   BestFaceIndex = -1;
    FVector BestLocalNormal;

    float tRoot = 0.0f;
    const auto& Nodes = BVH.GetNodes();
    const auto& ObjIndices = BVH.GetObjectIndices();
    const int32 RootIdx = BVH.GetRootNodeIndex();

    if (RootIdx == -1 || !Nodes[RootIdx].Bounds.IntersectRay(LocalRay, tRoot))
        return false;

    Stack.push_back({RootIdx, tRoot});

    while (!Stack.empty())
    {
        auto [NodeIdx, tEnter] = Stack.back();
        Stack.pop_back();

        if (tEnter >= ClosestT) continue;   // ★ Early Out

        const FBVH::FNode& Node = Nodes[NodeIdx];

        if (Node.IsLeaf())
        {
            for (int32 k = 0; k < Node.ObjectCount; ++k)
            {
                const int32  TriIdx = ObjIndices[Node.FirstObject + k];
                const uint32 i      = static_cast<uint32>(TriIdx) * 3;
                if (i + 2 >= Indices.size()) continue;

                const FVector& V0 = Vertices[Indices[i    ]].Position;
                const FVector& V1 = Vertices[Indices[i + 1]].Position;
                const FVector& V2 = Vertices[Indices[i + 2]].Position;

				// ClosestT 갱신
                float HitT = 0.0f;
                if (IntersectTriangle(LocalRay.Origin, LocalRay.Direction, V0, V1, V2, HitT)
                    && HitT >= 0.0f && HitT < ClosestT)
                {
                    ClosestT = HitT;
                    BestFaceIndex = static_cast<int32>(TriIdx);
                    BestLocalNormal = FVector::CrossProduct(V1 - V0, V2 - V0).GetSafeNormal();
                }
            }
            continue;
        }

        float tL = FLT_MAX, tR = FLT_MAX;
        bool bL = (Node.Left  != -1) && Nodes[Node.Left ].Bounds.IntersectRay(LocalRay, tL) && tL < ClosestT;
        bool bR = (Node.Right != -1) && Nodes[Node.Right].Bounds.IntersectRay(LocalRay, tR) && tR < ClosestT;

        // 가까운 노드를 나중에 push → 먼저 pop (Front-to-Back 순서 보장)
        if (bL && bR)
        {
            if (tL < tR) { Stack.push_back({Node.Right, tR}); Stack.push_back({Node.Left,  tL}); }
            else         { Stack.push_back({Node.Left,  tL}); Stack.push_back({Node.Right, tR}); }
        }
        else if (bL) Stack.push_back({Node.Left,  tL});
        else if (bR) Stack.push_back({Node.Right, tR});
    }

    if (BestFaceIndex < 0) return false;
	
	const FVector LocalHitLocation = LocalRay.Origin + LocalRay.Direction * ClosestT;
	const FVector WorldHitLocation = GetWorldMatrix().TransformPosition(LocalHitLocation);
	FVector WorldNormal = GetWorldMatrix().TransformVector(BestLocalNormal);
	WorldNormal.NormalizeSafe();
	
	OutHitResult.bHit = true;
	OutHitResult.HitComponent = this;
	OutHitResult.Distance = (WorldHitLocation - Ray.Origin).Size();
	OutHitResult.Location = WorldHitLocation;
	OutHitResult.Normal = WorldNormal;
	OutHitResult.FaceIndex = BestFaceIndex;
	
	return true;
}

const FAABB& UStaticMeshComponent::GetWorldAABB() const
{
	EnsureBoundsUpdated();
	return WorldAABB;
}

bool UStaticMeshComponent::ConsumeRenderStateDirty()
{
	const bool bWasDirty = bRenderStateDirty;
	bRenderStateDirty = false;
	return bWasDirty;
}

void UStaticMeshComponent::MarkBoundsDirty()
{
	bBoundsDirty = true;
}

void UStaticMeshComponent::MarkRenderStateDirty()
{
	bRenderStateDirty = true;
}

void UStaticMeshComponent::EnsureBoundsUpdated() const
{
	if (!bBoundsDirty && !bTransformDirty)
	{
		return;
	}

	if (bTransformDirty)
	{
		GetWorldMatrix();
	}

	const_cast<UStaticMeshComponent*>(this)->UpdateWorldAABB();
}