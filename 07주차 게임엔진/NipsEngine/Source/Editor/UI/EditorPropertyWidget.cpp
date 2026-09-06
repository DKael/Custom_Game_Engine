#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/EditorEngine.h"
#include "ImGui/imgui.h"
#include "Component/StaticMeshComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/GizmoComponent.h"
#include "Core/PropertyTypes.h"
#include "Core/ResourceManager.h"
#include "Object/FName.h"
#include <cmath>
#include <functional>
#include "Component/SubUVComponent.h"
#include "Selection/SelectionManager.h"
#include "Component/FireBallComponent.h"
#include "Component/ProjectileComponent.h"
#include "Component/RotationMovementComponent.h"
#include "Component/SpawnRandomRotatingCopiesComponent.h"
#include "Component/Light/LightComponent.h"

#include "Component/Light/SpotLightComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Light/PointLightComponent.h"


#define SEPARATOR()                                                                                                    \
    ;                                                                                                                  \
    ImGui::Spacing();                                                                                                  \
    ImGui::Spacing();                                                                                                  \
    ImGui::Separator();                                                                                                \
    ImGui::Spacing();                                                                                                  \
    ImGui::Spacing();

// 1. 메뉴 항목의 이름과, 해당 컴포넌트를 생성&초기화할 함수(람다)를 담는 구조체
struct FComponentMenuEntry
{
    const char*                              DisplayName;
    std::function<UActorComponent*(AActor*)> CreateAndInitFunc;
};

// 2. 에디터에서 추가 가능한 컴포넌트 배열 (이 리스트만 관리하면 됩니다)
static const TArray<FComponentMenuEntry> ComponentMenuRegistry = {
	{"StaticMesh Component",
	 [](AActor* Actor) -> USceneComponent* { return Actor->AddComponent<UStaticMeshComponent>(); }},
	{"SubUV Component",
	 [](AActor* Actor) -> USceneComponent*
	 {
		 USubUVComponent* Comp = Actor->AddComponent<USubUVComponent>();
		 Comp->SetParticle(FName("Explosion"));
		 Comp->SetSpriteSize(2.0f, 2.0f);
		 Comp->SetFrameRate(30.f);
		 return Comp;
	 }},
	{"TextRender Component",
	 [](AActor* Actor) -> USceneComponent*
	 {
		 UTextRenderComponent* Comp = Actor->AddComponent<UTextRenderComponent>();
		 Comp->SetFont(FName("Default"));
		 Comp->SetText("TextRender");
		 return Comp;
	 }},
	{"Billboard Component",
	 [](AActor* Actor) -> USceneComponent*
	 {
		 UBillboardComponent* Comp = Actor->AddComponent<UBillboardComponent>();
		 Comp->SetTextureName("Asset/Texture/Pawn_64x.png");
		 return Comp;
	 }},
	{"Decal Component",
	 [](AActor* Actor) -> USceneComponent*
	 {
		 UDecalComponent* Comp = Actor->AddComponent<UDecalComponent>();
		 FMaterial* DefaultDecalMat = FResourceManager::Get().FindMaterial("DicePaper");
		 Comp->SetDecalMaterial(DefaultDecalMat);
		 return Comp;
	 }},
	{"FireBall Component",
	 [](AActor* Actor) -> USceneComponent*
	 {
		 UFireBallComponent* Comp = Actor->AddComponent<UFireBallComponent>();
		 return Comp;
	 }},
	{"ProjectileMovement Component",
	 [](AActor* Actor) -> UActorComponent*
	 {
		 UProjectileMovementComponent* Comp = Actor->AddComponent<UProjectileMovementComponent>();
		 return Comp;
	 }},
	{"RotationMovement Component",
	 [](AActor* Actor) -> UActorComponent*
	 {
		 URotationMovementComponent* Comp = Actor->AddComponent<URotationMovementComponent>();
		 return Comp;
	 }},
	{"SpawnRandomRotatingCopies Component",
	 [](AActor* Actor) -> UActorComponent*
	 {
		 USpawnRandomRotatingCopiesComponent* Comp = Actor->AddComponent<USpawnRandomRotatingCopiesComponent>();
		 return Comp;
	 }},

	{"SpotLight Component", 
	[](AActor* Actor) -> UActorComponent*
     {
         USpotLightComponent* Comp = Actor->AddComponent<USpotLightComponent>();
         return Comp;
     }},

    {"DirectionalLight Component", [](AActor* Actor) -> UActorComponent*
     {
         UDirectionalLightComponent* Comp = Actor->AddComponent<UDirectionalLightComponent>();
         return Comp;
     }},

	{"PointLight Component", [](AActor* Actor) -> UActorComponent*
	 {
		 UPointLightComponent* Comp = Actor->AddComponent<UPointLightComponent>();
		 return Comp;
     }},
};

namespace
{
	float SRGBToLinear(float Value)
	{
		Value = Value < 0.0f ? 0.0f : (Value > 1.0f ? 1.0f : Value);
		if (Value <= 0.04045f)
		{
			return Value / 12.92f;
		}

		return std::pow((Value + 0.055f) / 1.055f, 2.4f);
	}

	float LinearToSRGB(float Value)
	{
		Value = Value < 0.0f ? 0.0f : Value;
		if (Value <= 0.0031308f)
		{
			return Value * 12.92f;
		}

		return 1.055f * std::pow(Value, 1.0f / 2.4f) - 0.055f;
	}
}

UActorComponent* FEditorPropertyWidget::FindPreferredComponentForActor(AActor* Actor) const
{
    if (Actor == nullptr)
    {
        return nullptr;
    }

    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (Component != nullptr && Component->IsA<ULightComponent>())
        {
            return Component;
        }
    }

    USceneComponent* RootComp = Actor->GetRootComponent();
    if (RootComp != nullptr && RootComp->IsA<UStaticMeshComponent>())
    {
        return RootComp;
    }

    return nullptr;
}

void FEditorPropertyWidget::SyncSelectionTarget(AActor* PrimaryActor)
{
    if (PrimaryActor == nullptr)
    {
        SelectedComponent = nullptr;
        LastSelectedActor = nullptr;
        bActorSelected = true;
        return;
    }

    if (PrimaryActor != LastSelectedActor)
    {
        LastSelectedActor = PrimaryActor;
        SelectedComponent = FindPreferredComponentForActor(PrimaryActor);
        bActorSelected = (SelectedComponent == nullptr);
    }
}

void FEditorPropertyWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);

    ImGui::Begin("Jungle Property Window");

    AActor* PrimaryActor = SelectionManager->GetPrimarySelection();
    if (!PrimaryActor)
    {
        SyncSelectionTarget(nullptr);
        ImGui::Text("No object selected.");
        ImGui::End();
        return;
    }

    SyncSelectionTarget(PrimaryActor);

    const TArray<AActor*>& SelectedActors = SelectionManager->GetSelectedActors();
    const int32            SelectionCount = static_cast<int32>(SelectedActors.size());

    // ========== 고정 영역: Actor Info (clickable) ==========
    if (SelectionCount > 1)
    {
        ImGui::Text("Class: %s", PrimaryActor->GetTypeInfo()->name);

        FString PrimaryName = PrimaryActor->GetFName().ToString();
        if (PrimaryName.empty())
            PrimaryName = PrimaryActor->GetTypeInfo()->name;

        bool bHighlight = bActorSelected;
        if (bHighlight)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("Name: %s (+%d)", PrimaryName.c_str(), SelectionCount - 1);
        if (bHighlight)
            ImGui::PopStyleColor();
        if (ImGui::IsItemClicked())
        {
            bActorSelected = true;
            SelectedComponent = nullptr;
        }
        ImGui::SameLine();
        char RemoveLabel[64];
        snprintf(RemoveLabel, sizeof(RemoveLabel), "Remove %d Objects", SelectionCount);
        if (ImGui::Button(RemoveLabel))
        {
            for (AActor* Actor : SelectedActors)
            {
                if (Actor && Actor->GetWorld())
                {
                    Actor->GetWorld()->DestroyActor(Actor);
                }
            }
            SelectionManager->ClearSelection();
            SelectedComponent = nullptr;
            LastSelectedActor = nullptr;
            ImGui::End();
            return;
        }
    }
    else
    {
        ImGui::Text("Class: %s", PrimaryActor->GetTypeInfo()->name);

        // Actor 이름: 클릭 가능, 선택 시 하이라이트
        bool bHighlight = bActorSelected;
        if (bHighlight)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("Name: %s", PrimaryActor->GetFName().ToString().c_str());
        if (bHighlight)
            ImGui::PopStyleColor();
        if (ImGui::IsItemClicked())
        {
            bActorSelected = true;
            SelectedComponent = nullptr;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove"))
        {
            if (PrimaryActor->GetWorld())
            {
                PrimaryActor->GetWorld()->DestroyActor(PrimaryActor);
            }
            SelectionManager->ClearSelection();
            SelectedComponent = nullptr;
            LastSelectedActor = nullptr;
            ImGui::End();
            return;
        }

        ImGui::Spacing();

        if (ImGui::Button("Add Component", ImVec2(-1, 0)))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            for (const FComponentMenuEntry& Entry : ComponentMenuRegistry)
            {
                if (!ImGui::Selectable(Entry.DisplayName))
                    continue;

                UActorComponent* NewComp = Entry.CreateAndInitFunc(PrimaryActor);

                if (!NewComp)
                    continue;

                USceneComponent* SceneComp = Cast<USceneComponent>(NewComp);
                USceneComponent* RootComp = PrimaryActor->GetRootComponent();
                if (RootComp && SceneComp)
                    SceneComp->AttachToComponent(RootComp);
                else
                    PrimaryActor->SetRootComponent(SceneComp);

                SelectedComponent = NewComp;
            }
            ImGui::EndPopup();
        }
    }

    // ========== 고정 영역: Component Tree ==========
    SEPARATOR();
    RenderComponentTree(PrimaryActor);

    // ========== 스크롤 영역: Details ==========
    SEPARATOR();
    ImGui::Text("Details");
    ImGui::Separator();

    float ScrollHeight = ImGui::GetContentRegionAvail().y;
    if (ScrollHeight < 50.0f)
        ScrollHeight = 50.0f;

    ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        RenderDetails(PrimaryActor, SelectedActors);
    }
    ImGui::EndChild();

    ImGui::End();
}

void FEditorPropertyWidget::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorWidget::Initialize(InEditorEngine);
    SelectionManager = &EditorEngine->GetSelectionManager();
}

void FEditorPropertyWidget::RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
    if (bActorSelected)
    {
        RenderActorProperties(PrimaryActor, SelectedActors);
    }
    else if (SelectedComponent)
    {
        RenderComponentProperties();
    }
    else
    {
        ImGui::TextDisabled("Select an actor or component to view details.");
    }
}

void FEditorPropertyWidget::RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
    ImGui::Text("Actor: %s", PrimaryActor->GetTypeInfo()->name);
    ImGui::Text("Name: %s", PrimaryActor->GetFName().ToString().c_str());

    if (PrimaryActor->GetRootComponent())
    {
        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::Spacing();

        FVector Pos = PrimaryActor->GetActorLocation();
        float   PosArray[3] = {Pos.X, Pos.Y, Pos.Z};

        FVector Rot = PrimaryActor->GetActorRotation();
        float   RotArray[3] = {Rot.X, Rot.Y, Rot.Z};

        FVector Scale = PrimaryActor->GetActorScale();
        float   ScaleArray[3] = {Scale.X, Scale.Y, Scale.Z};

        if (ImGui::DragFloat3("Location", PosArray, 0.1f))
        {
            FVector Delta = FVector(PosArray[0], PosArray[1], PosArray[2]) - Pos;
            for (AActor* Actor : SelectedActors)
            {
                if (Actor)
                    Actor->AddActorWorldOffset(Delta);
            }
            EditorEngine->GetGizmo()->UpdateGizmoTransform();
        }
        if (ImGui::DragFloat3("Rotation", RotArray, 0.1f))
        {
            FVector Delta = FVector(RotArray[0], RotArray[1], RotArray[2]) - Rot;
            for (AActor* Actor : SelectedActors)
            {
                if (Actor)
                    Actor->SetActorRotation(Actor->GetActorRotation() + Delta);
            }
            EditorEngine->GetGizmo()->UpdateGizmoTransform();
        }
        if (ImGui::DragFloat3("Scale", ScaleArray, 0.1f))
        {
            FVector Delta = FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]) - Scale;
            for (AActor* Actor : SelectedActors)
            {
                if (Actor)
                    Actor->SetActorScale(Actor->GetActorScale() + Delta);
            }
        }
    }

    ImGui::Separator();
    bool bVisible = PrimaryActor->IsVisible();
    if (ImGui::Checkbox("Visible", &bVisible))
    {
        PrimaryActor->SetVisible(bVisible);
    }

    ImGui::Separator();
    // Billboard 타입 체크
    if (UBillboardComponent* BillboardComp = dynamic_cast<UBillboardComponent*>(PrimaryActor->GetRootComponent()))
    {
        if (dynamic_cast<USubUVComponent*>(PrimaryActor->GetRootComponent()))
        {
            return;
        }
        ImGui::Separator();
        ImGui::Text("Sprite Texture");

        const TArray<FString>& TextureList = FResourceManager::Get().GetTextureFilePath();
        const FString          CurrentName = BillboardComp->GetTextureName();

        if (ImGui::BeginCombo("##SpriteTexture", CurrentName.empty() ? "None" : CurrentName.c_str()))
        {
            for (const FString& TexPath : TextureList)
            {
                // 경로 전체 대신 파일명만 표시
                FString DisplayName = TexPath;
                bool    bSelected = (TexPath == CurrentName);

                if (ImGui::Selectable(DisplayName.c_str(), bSelected))
                {
                    for (AActor* Actor : SelectedActors)
                    {
                        if (UBillboardComponent* Comp = dynamic_cast<UBillboardComponent*>(Actor->GetRootComponent()))
                        {
                            Comp->SetTextureName(TexPath);
                        }
                    }
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
}

void FEditorPropertyWidget::RenderComponentTree(AActor* Actor)
{
    ImGui::Text("Components");
    ImGui::Separator();

    USceneComponent* Root = Actor->GetRootComponent();

    if (Root)
    {
        RenderSceneComponentNode(Root);
    }

    // Non-scene ActorComponents
    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (!Comp)
            continue;
        if (Comp->IsA<USceneComponent>())
            continue;

        FString Name = Comp->GetFName().ToString();
        if (Name.empty())
            Name = Comp->GetTypeInfo()->name;

        ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (!bActorSelected && SelectedComponent == Comp)
            Flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::TreeNodeEx(Comp, Flags, "[%s] %s", Comp->GetTypeInfo()->name, Name.c_str());
        if (ImGui::IsItemClicked())
        {
            SelectedComponent = Comp;
            bActorSelected = false;
        }
    }
}

void FEditorPropertyWidget::RenderSceneComponentNode(USceneComponent* Comp)
{
    if (!Comp)
        return;

    FString Name = Comp->GetFName().ToString();
    if (Name.empty())
        Name = Comp->GetTypeInfo()->name;

    const auto& Children = Comp->GetChildren();
    bool        bHasChildren = !Children.empty();

    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (!bHasChildren)
        Flags |= ImGuiTreeNodeFlags_Leaf;
    if (!bActorSelected && SelectedComponent == Comp)
        Flags |= ImGuiTreeNodeFlags_Selected;

    bool bIsRoot = (Comp->GetParent() == nullptr);
    bool bOpen =
        ImGui::TreeNodeEx(Comp, Flags, "%s%s (%s)", bIsRoot ? "[Root] " : "", Name.c_str(), Comp->GetTypeInfo()->name);

    if (ImGui::IsItemClicked())
    {
        SelectedComponent = Comp;
        bActorSelected = false;
    }

    if (bOpen)
    {
        for (USceneComponent* Child : Children)
        {
            RenderSceneComponentNode(Child);
        }
        ImGui::TreePop();
    }
}

void FEditorPropertyWidget::RenderComponentProperties()
{
    ImGui::Text("Component: %s", SelectedComponent->GetTypeInfo()->name);
    ImGui::Text("Name: %s", SelectedComponent->GetFName().ToString().c_str());
    ImGui::Separator();

    // PropertyDescriptor 기반 자동 위젯 렌더링
    TArray<FPropertyDescriptor> Props;
    SelectedComponent->GetEditableProperties(Props);

    bool bIsRoot = false;
    if (SelectedComponent->IsA<USceneComponent>())
    {
        USceneComponent* SceneComp = static_cast<USceneComponent*>(SelectedComponent);
        bIsRoot = (SceneComp->GetParent() == nullptr);
    }

    // Pass 1: Transform 프로퍼티 먼저 (Root가 아닐 때만)
    for (auto& Prop : Props)
    {
        RenderPropertyWidget(Prop);
    }
    ImGui::Separator();

    // 프로퍼티 직접 편집 후 월드 행렬 갱신
    if (SelectedComponent->IsA<USceneComponent>())
    {
        static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
        SelectionManager->GetGizmo()->UpdateGizmoTransform();
    }
}

void FEditorPropertyWidget::RenderPropertyWidget(FPropertyDescriptor& Prop)
{
    bool bChanged = false;

	switch (Prop.Type)
	{
	case EPropertyType::Bool:
	{
		bool* Val = static_cast<bool*>(Prop.ValuePtr);
		bChanged = ImGui::Checkbox(Prop.Name, Val);
		break;
	}
	case EPropertyType::Int:
	{
		int32* Val = static_cast<int32*>(Prop.ValuePtr);
            if (strstr(Prop.Name, "Sort Order") != nullptr)
            {
                // step = 1, step_fast = 5 (쉬프트 누르고 클릭시)
                bChanged = ImGui::InputInt(Prop.Name, Val, 1, 5);
            }
            else
            {
                bChanged = ImGui::DragInt(Prop.Name, Val);
            }
		break;
	}
	case EPropertyType::Float:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		if (Prop.Min != 0.0f || Prop.Max != 0.0f)
			bChanged = ImGui::DragFloat(Prop.Name, Val, Prop.Speed, Prop.Min, Prop.Max);
		else
			bChanged = ImGui::DragFloat(Prop.Name, Val, Prop.Speed);
		break;
	}
	case EPropertyType::Vec3:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		bChanged = ImGui::DragFloat3(Prop.Name, Val, Prop.Speed);
		break;
	}
	case EPropertyType::Vec4:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		bChanged = ImGui::ColorEdit4(Prop.Name, Val);
		break;
	}
	case EPropertyType::LinearColor:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		float DisplayColor[4] =
		{
			LinearToSRGB(Val[0]),
			LinearToSRGB(Val[1]),
			LinearToSRGB(Val[2]),
			Val[3]
		};

		bChanged = ImGui::ColorEdit4(Prop.Name, DisplayColor);
		if (bChanged)
		{
			Val[0] = SRGBToLinear(DisplayColor[0]);
			Val[1] = SRGBToLinear(DisplayColor[1]);
			Val[2] = SRGBToLinear(DisplayColor[2]);
			Val[3] = DisplayColor[3];
		}
		break;
	}
	case EPropertyType::String:
	{
		FString* Val = static_cast<FString*>(Prop.ValuePtr);

        if (strcmp(Prop.Name, "StaticMesh") == 0)
        {
            TArray<FString> MeshPaths = FResourceManager::Get().GetStaticMeshPaths();
            if (!MeshPaths.empty())
            {
                const FString Current = *Val;
                if (ImGui::BeginCombo(Prop.Name, Current.empty() ? "<None>" : Current.c_str()))
                {
                    for (const FString& Path : MeshPaths)
                    {
                        const bool bSelected = (Current == Path);
                        if (ImGui::Selectable(Path.c_str(), bSelected))
                        {
                            *Val = Path;
                            bChanged = true;
                        }
                        if (bSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }
        else
        {
            const bool bIsMaterialSlot = (strncmp(Prop.Name, "Material ", 9) == 0);
            if (bIsMaterialSlot)
            {
                ImGui::BeginDisabled();

                char Buf[256];
                strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
                ImGui::InputText(Prop.Name, Buf, sizeof(Buf));

                ImGui::EndDisabled();
                break;
            }
        }
        break;
    }
    case EPropertyType::Name:
    {
        FName*  Val = static_cast<FName*>(Prop.ValuePtr);
        FString Current = Val->ToString();

        // 리소스 키와 매칭되는 프로퍼티면 콤보 박스로 렌더링
        TArray<FString> Names;
        if (strcmp(Prop.Name, "Font") == 0)
            Names = FResourceManager::Get().GetFontNames();
        else if (strcmp(Prop.Name, "Particle") == 0)
            Names = FResourceManager::Get().GetParticleNames();
        else if (strcmp(Prop.Name, "Sprite") == 0)
            Names = FResourceManager::Get().GetTextureFilePath();

        if (!Names.empty())
        {
            if (ImGui::BeginCombo(Prop.Name, Current.c_str()))
            {
                for (const auto& Name : Names)
                {
                    bool bSelected = (Current == Name);
                    if (ImGui::Selectable(Name.c_str(), bSelected))
                    {
                        *Val = FName(Name);
                        bChanged = true;
                    }
                    if (bSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            char Buf[256];
            strncpy_s(Buf, sizeof(Buf), Current.c_str(), _TRUNCATE);
            if (ImGui::InputText(Prop.Name, Buf, sizeof(Buf)))
            {
                *Val = FName(Buf);
                bChanged = true;
            }
        }
        break;
    }
    }

    if (bChanged && SelectedComponent)
    {
        SelectedComponent->PostEditProperty(Prop.Name);
    }
}
