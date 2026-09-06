#include "MeshComponent.h"

#include "Core/ResourceManager.h"

DEFINE_CLASS(UMeshComponent, UPrimitiveComponent)

// UpdateWorldAABB 등의 함수를 오버라이드하지 않았기 때문에 UMeshComponent도 추상 클래스가 됩니다.
// 추후에 MeshComponent를 사용할 일이 있다면 Duplicate의 주석을 해제하고 수정하시면 됩니다.

// 부모 클래스를 계층별로 복사한 뒤, Matrerial을 얕은 복사로 세팅해 줍니다.
//UMeshComponent* UMeshComponent::Duplicate()
//{
//    UMeshComponent* NewComp = UObjectManager::Get().CreateObject<UMeshComponent>();
//
//    NewComp->SetActive(this->IsActive());
//    NewComp->SetOwner(nullptr);
//    
//    NewComp->SetRelativeLocation(this->GetRelativeLocation());
//    NewComp->SetRelativeRotation(this->GetRelativeRotation());
//    NewComp->SetRelativeScale(this->GetRelativeScale());
//
//    NewComp->SetVisibility(this->IsVisible());
//  
//    NewComp->OverrideMaterial = this->OverrideMaterial;
//    NewComp->ScrollUV = this->ScrollUV;
//
//    return NewComp;
//}

void UMeshComponent::SetMaterial(int32 SlotIndex, FMaterial* InMaterial)
{
	if (SlotIndex < 0)
	{
		return;
	}
	
	if (SlotIndex >= static_cast<int32>(OverrideMaterial.size()))
	{
		OverrideMaterial.resize(SlotIndex + 1, nullptr);
	}

	OverrideMaterial[SlotIndex] = InMaterial;
}

FMaterial* UMeshComponent::GetMaterial(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(OverrideMaterial.size()))
	{
		return nullptr;
	}
	
	return OverrideMaterial[SlotIndex];
}

const TArray<FMaterial*>& UMeshComponent::GetOverrideMaterial() const
{
	return OverrideMaterial;
}

int32 UMeshComponent::GetMaterialCount() const
{
	return static_cast<int32>(OverrideMaterial.size());
}

void UMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Scroll U", EPropertyType::Float, &ScrollUV.first,  -1.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Scroll V", EPropertyType::Float, &ScrollUV.second, -1.0f, 1.0f, 0.01f });

	// 🔥 다중 매테리얼 직렬화 처리
	int32 MatCount = GetMaterialCount();

	// 버퍼 배열 크기를 현재 슬롯 개수에 맞게 늘림
	SerializedMaterialNames.resize(MatCount);
	MaterialPropertyNames.resize(MatCount);

	for (int32 i = 0; i < MatCount; ++i)
	{
		// 1. 현재 슬롯의 매테리얼 이름 동기화
		if (OverrideMaterial[i] != nullptr) {
			SerializedMaterialNames[i] = OverrideMaterial[i]->Name;
		}
		else {
			SerializedMaterialNames[i] = "";
		}

		// 2. "Material 0", "Material 1" 등의 프로퍼티 이름 동적 생성
		MaterialPropertyNames[i] = "Material " + std::to_string(i);

		// 3. 만들어진 이름과 변수를 직렬화 시스템에 연결
		OutProps.push_back({ MaterialPropertyNames[i].c_str(), EPropertyType::String, &SerializedMaterialNames[i] });
	}
}

void UMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	// 🔥 "Material " 로 시작하는지 검사 (9글자 일치 확인)
	if (std::strncmp(PropertyName, "Material ", 9) == 0)
	{
		// 문자열에서 인덱스 숫자만 추출 ("Material 12" -> 12)
		int32 SlotIndex = std::stoi(PropertyName + 9);

		if (SlotIndex >= 0 && SlotIndex < static_cast<int32>(SerializedMaterialNames.size()))
		{
			FString MatName = SerializedMaterialNames[SlotIndex];
			if (!MatName.empty())
			{
				FMaterial* Mat = FResourceManager::Get().FindMaterial(MatName);
				if (Mat)
				{
					SetMaterial(SlotIndex, Mat); // 해당 슬롯에 정확히 세팅!
				}
			}
		}
	}
}
void UMeshComponent::TickComponent(float DeltaTime)
{
	//ScrollUV.second += DeltaTime;

	//if (ScrollUV.first >= 1.f) ScrollUV.first = 0.f;
}

