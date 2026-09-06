#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>

#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "GameFramework/PrimitiveActors.h"
#include "Component/SceneComponent.h"
#include "Component/ActorComponent.h"
#include "Component/TextRenderComponent.h"
#include "Object/Object.h"
#include "Object/ActorIterator.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Object/FName.h"
#include "Math/Matrix.h"

namespace SceneKeys
{
	static constexpr const char* Version            = "Version";
	static constexpr const char* Name               = "Name";
	static constexpr const char* ClassName          = "ClassName";
	static constexpr const char* WorldType          = "WorldType";
	static constexpr const char* ContextName        = "ContextName";
	static constexpr const char* ContextHandle      = "ContextHandle";
	static constexpr const char* Actors             = "Actors";
	static constexpr const char* Visible            = "Visible";
	static constexpr const char* RootComponent      = "RootComponent";
	static constexpr const char* NonSceneComponents = "NonSceneComponents";
	static constexpr const char* Properties         = "Properties";
	static constexpr const char* Children           = "Children";

	// PerspectiveCamera 섹션
	static constexpr const char* PerspectiveCamera  = "PerspectiveCamera";
	static constexpr const char* Primitives         = "Primitives";
	static constexpr const char* Scale              = "Scale";
	static constexpr const char* Location           = "Location";
	static constexpr const char* Rotation           = "Rotation";
	static constexpr const char* FOV                = "FOV";
	static constexpr const char* NearClip           = "NearClip";
	static constexpr const char* FarClip            = "FarClip";
	static constexpr const char* Type               = "Type";
}

static const char* WorldTypeToString(EWorldType Type)
{
	switch (Type) {
	case EWorldType::Game: return "Game";
	case EWorldType::PIE:  return "PIE";
	default:               return "Editor";
	}
}

static EWorldType StringToWorldType(const string& Str)
{
	if (Str == "Game") return EWorldType::Game;
	if (Str == "PIE")  return EWorldType::PIE;
	return EWorldType::Editor;
}

// ============================================================
// Save
// ============================================================

void FSceneSaveManager::SaveSceneAsJSON(const string& InSceneName, FWorldContext& WorldContext,
                                        const FEditorCameraState* CameraState)
{
    using namespace json;
    if (!WorldContext.World) return;

    string FinalName = InSceneName.empty() ? "Save_" + GetCurrentTimeStamp() : InSceneName;
    std::wstring SceneDir = GetSceneDirectory();
    std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);
    std::filesystem::create_directories(SceneDir);

    JSON Root = json::Object();
    Root[SceneKeys::Version] = 3; 
    Root[SceneKeys::Name] = FinalName;

    JSON WorldData = SerializeWorld(WorldContext.World, WorldContext);

    Root[SceneKeys::ClassName] = WorldContext.World->GetTypeInfo()->name;
    Root[SceneKeys::WorldType] = WorldTypeToString(WorldContext.WorldType);
    Root[SceneKeys::PerspectiveCamera] = SerializeCameraState(CameraState);

    Root[SceneKeys::Actors] = WorldData[SceneKeys::Actors];

    std::ofstream File(FileDestination);
    if (File.is_open()) {
        File << Root.dump();
        File.flush();
        File.close();
    }
}

json::JSON FSceneSaveManager::SerializeWorldToPrimitives(UWorld* World, const FWorldContext& Ctx)
{
    using namespace json;
    JSON Primitives = json::Object();
    int32 PrimitiveID = 0;

	if (ULevel* PersistentLevel = World->GetPersistentLevel())
    {
        for (AActor* Actor : PersistentLevel->GetActors())
        {
            if (!Actor) continue;

            // 루트 컴포넌트만 추출하여 직렬화
            if (USceneComponent* RootComp = Actor->GetRootComponent()) 
            {
                JSON PrimObj = SerializeComponentToPrimitive(RootComp);
                Primitives[std::to_string(PrimitiveID++)] = PrimObj;
            }
        }
    }
    return Primitives;
}

json::JSON FSceneSaveManager::SerializeComponentToPrimitive(USceneComponent* SceneComp)
{
    using namespace json;
    JSON PrimObj = json::Object();

    FString ClassName = SceneComp->GetTypeInfo()->name;
    if (ClassName == "UStaticMeshComponent") { ClassName = "StaticMeshComp"; }
    PrimObj[SceneKeys::Type] = ClassName;

    TArray<FPropertyDescriptor> Descriptors;
    SceneComp->GetEditableProperties(Descriptors);
    for (const auto& Prop : Descriptors) 
    {
        FString OutKey = Prop.Name;
        if (strcmp(Prop.Name, "StaticMesh") == 0) { OutKey = "ObjStaticMeshAsset"; }
        
        PrimObj[OutKey] = SerializePropertyValue(Prop);
    }

    return PrimObj;
}

/* @brief 현재 사용하지 않는 함수, 추후 Actor-Component 단위로 계층화를 시켜야 한다면 이쪽을 사용 */
json::JSON FSceneSaveManager::SerializeWorld(UWorld* World, const FWorldContext& Ctx)
{
	using namespace json;
	JSON w = json::Object();
	w[SceneKeys::ClassName] = World->GetTypeInfo()->name;
	w[SceneKeys::WorldType] = WorldTypeToString(Ctx.WorldType);
	w[SceneKeys::ContextName] = Ctx.ContextName;
	w[SceneKeys::ContextHandle] = Ctx.ContextHandle.ToString();

	JSON Actors = json::Array();
	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if (!Actor) continue;
		Actors.append(SerializeActor(Actor));
	}
	w[SceneKeys::Actors] = Actors;
	return w;
}

json::JSON FSceneSaveManager::SerializeActor(AActor* Actor)
{
	using namespace json;
	JSON a = json::Object();
	a[SceneKeys::ClassName] = Actor->GetTypeInfo()->name;
	a[SceneKeys::Visible] = Actor->IsVisible();

	// 자식 컴포넌트 및 NonScene 컴포넌트는 무시하고 RootComponent만 직렬화
	if (USceneComponent* Root = Actor->GetRootComponent()) 
	{
		a[SceneKeys::RootComponent] = SerializeSceneComponentTree(Root);
	}

	JSON NonSceneComponents = json::Array();
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (Comp && Cast<USceneComponent>(Comp) == nullptr)
		{
			NonSceneComponents.append(SerializeActorComponent(Comp));
		}
	}

	if (NonSceneComponents.size() > 0)
	{
		a[SceneKeys::NonSceneComponents] = NonSceneComponents;
	}

	return a;
}

json::JSON FSceneSaveManager::SerializeSceneComponentTree(USceneComponent* Comp)
{
	using namespace json;
	JSON c = json::Object();
	
	FString ClassName = Comp->GetTypeInfo()->name;
	if (ClassName == "UStaticMeshComponent") { ClassName = "StaticMeshComp"; }
	c[SceneKeys::ClassName] = ClassName;
	c[SceneKeys::Properties] = SerializeProperties(Comp);

	JSON ChildrenArr = json::Array();

	for (USceneComponent* Child : Comp->GetChildren()) 
	{
	    if (Child)
	    {
		ChildrenArr.append(SerializeSceneComponentTree(Child));
	    }
	}
	if (ChildrenArr.size() > 0)
	{
	    c[SceneKeys::Children] = ChildrenArr;
	}
	
	return c;
}

json::JSON FSceneSaveManager::SerializeActorComponent(UActorComponent* Comp)
{
	using namespace json;
	JSON c = json::Object();
	c[SceneKeys::ClassName] = Comp->GetTypeInfo()->name;
	c[SceneKeys::Properties] = SerializeProperties(Comp);
	return c;
}

json::JSON FSceneSaveManager::SerializeProperties(UActorComponent* Comp)
{
	using namespace json;
	JSON props = json::Object();

	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	for (const auto& Prop : Descriptors) {
		props[Prop.Name] = SerializePropertyValue(Prop);
	}
	return props;
}

json::JSON FSceneSaveManager::SerializePropertyValue(const FPropertyDescriptor& Prop)
{
	using namespace json;

	switch (Prop.Type) {
	case EPropertyType::Bool:
		return JSON(*static_cast<bool*>(Prop.ValuePtr));

	case EPropertyType::Int:
		return JSON(*static_cast<int32*>(Prop.ValuePtr));

	case EPropertyType::Float:
		return JSON(static_cast<double>(*static_cast<float*>(Prop.ValuePtr)));

	case EPropertyType::Vec3: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		return arr;
	}
	case EPropertyType::Vec4:
	case EPropertyType::LinearColor: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		arr.append(static_cast<double>(v[3]));
		return arr;
	}
	case EPropertyType::String:
		return JSON(*static_cast<FString*>(Prop.ValuePtr));

	case EPropertyType::Name:
		return JSON(static_cast<FName*>(Prop.ValuePtr)->ToString());

	default:
		return JSON();
	}
}

json::JSON FSceneSaveManager::SerializeCameraState(const FEditorCameraState* CameraState /*= nullptr*/)
{
	using namespace json;

	// Perspective 카메라 상태 저장
	if (CameraState && CameraState->bValid)
	{
		JSON Cam = Object();
		Cam[SceneKeys::Location] = Array(
			static_cast<double>(CameraState->Location.X),
			static_cast<double>(CameraState->Location.Y),
			static_cast<double>(CameraState->Location.Z));
		Cam[SceneKeys::Rotation] = Array(
			static_cast<double>(CameraState->Rotation.Pitch),
			static_cast<double>(CameraState->Rotation.Yaw),
			static_cast<double>(CameraState->Rotation.Roll));
		
		Cam[SceneKeys::FOV] = Array(static_cast<double>(CameraState->FOV));
		Cam[SceneKeys::NearClip] = Array(static_cast<double>(CameraState->NearClip));
		Cam[SceneKeys::FarClip] = Array(static_cast<double>(CameraState->FarClip));
		
		return Cam;
	}
	return nullptr;
}

void FSceneSaveManager::DeserializeActorsToWorld(json::JSON& ActorsNode, UWorld* World)
{
	for (auto& ActorJSON : ActorsNode.ArrayRange())
	{
		if (!ActorJSON.hasKey(SceneKeys::ClassName)) continue;
		string ActorClassName = ActorJSON[SceneKeys::ClassName].ToString();

		// 1. Actor 인스턴스 생성
		UObject* Obj = FObjectFactory::Get().Create(ActorClassName);
		AActor* NewActor = Obj ? Cast<AActor>(Obj) : nullptr;
		if (!NewActor) continue;

		// 💡 2. C++ 뼈대 생성 (매우 중요! 여기서 빌보드, 데칼 등이 기본 세팅됨)
		NewActor->InitDefaultComponents();

		NewActor->SetWorld(World);

		if (ActorJSON.hasKey(SceneKeys::Visible)) {
			NewActor->SetVisible(ActorJSON[SceneKeys::Visible].ToBool());
		}

		if (ULevel* PersistentLevel = World->GetPersistentLevel()) {
			PersistentLevel->AddActor(NewActor);
		}

		// 💡 3. 루트 컴포넌트를 시작으로 트리 전체 복원 (덮어쓰기)
		if (ActorJSON.hasKey(SceneKeys::RootComponent))
		{
			auto RootNode = ActorJSON[SceneKeys::RootComponent];

			// 기존 루트 컴포넌트를 인자로 넘겨줌!
			USceneComponent* RootComp = DeserializeSceneComponentTree(RootNode, NewActor, NewActor->GetRootComponent());

			// 혹시라도 기존 루트가 없어서 새로 만들어진 경우에만 SetRootComponent
			if (RootComp && RootComp != NewActor->GetRootComponent())
			{
				NewActor->SetRootComponent(RootComp);
			}
		}

		if (ActorJSON.hasKey(SceneKeys::NonSceneComponents))
		{
			auto NonSceneNode = ActorJSON[SceneKeys::NonSceneComponents];
			DeserializeNonSceneComponents(NonSceneNode, NewActor);
		}
	}

	if (World != nullptr) {
		World->SyncSpatialIndex();
	}
}

// ============================================================
// Load
// ============================================================

void FSceneSaveManager::LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext, FEditorCameraState* OutCameraState)
{
    using json::JSON;
    std::ifstream File(std::filesystem::path(FPaths::ToWide(filepath)));
    if (!File.is_open()) return;

    string FileContent((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    JSON root = JSON::Load(FileContent);

    string ClassName = root.hasKey(SceneKeys::ClassName) ? root[SceneKeys::ClassName].ToString() : "UWorld";
    UObject* WorldObj = FObjectFactory::Get().Create(ClassName);
    if (!WorldObj || !WorldObj->IsA<UWorld>()) return;

    UWorld* World = static_cast<UWorld*>(WorldObj);
    EWorldType WorldType = root.hasKey(SceneKeys::WorldType) ? StringToWorldType(root[SceneKeys::WorldType].ToString()) : EWorldType::Editor;

    DeserializeCameraState(root, OutCameraState);

    // Primitives 파싱
    if (root.hasKey(SceneKeys::Actors)) 
    {
        DeserializeActorsToWorld(root[SceneKeys::Actors], World);
    }

    OutWorldContext.WorldType = WorldType;
    OutWorldContext.World = World;
}
#include <functional>
#include <unordered_map>

void FSceneSaveManager::DeserializePrimitivesToWorld(json::JSON& PrimitivesNode, UWorld* World)
{
    for (auto& Pair : PrimitivesNode.ObjectRange()) 
    {
        json::JSON& PrimJSON = Pair.second;

        if (!PrimJSON.hasKey(SceneKeys::Type)) continue;
        string CompType = PrimJSON[SceneKeys::Type].ToString();

		// StaticMeshComponent 예외 처리
        if (CompType == "StaticMeshComp") CompType = "UStaticMeshComponent";

        string ActorClassName = "AActor"; 
        if (CompType.front() == 'U' && CompType.substr(CompType.length() - 9) == "Component") 
        {
            string BaseName = CompType.substr(1, CompType.length() - 10);
            ActorClassName = "A" + BaseName + "Actor";
        }
        UObject* Obj = FObjectFactory::Get().Create(ActorClassName);

        AActor* NewActor = Obj ? Cast<AActor>(Obj) : nullptr;
        if (!NewActor) continue;

        NewActor->InitDefaultComponents();
        NewActor->SetWorld(World);
        //World->SpawnActor(NewActor);
		
		if (ULevel* PersistentLevel = World->GetPersistentLevel())
        {
            PersistentLevel->AddActor(NewActor);
        }

        if (USceneComponent* RootComp = NewActor->GetRootComponent()) 
        {
            DeserializeProperties(RootComp, PrimJSON);
            RootComp->MarkTransformDirty();
        }
    }

    if (World != nullptr)
    {
        World->SyncSpatialIndex();
    }
}

/* @brief 현재 사용하지 않는 함수, 추후 Actor-Component 단위로 계층화를 시켜야 한다면 이쪽을 사용 */
USceneComponent* FSceneSaveManager::DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner, USceneComponent* ExistingComp)
{
	if (!Node.hasKey(SceneKeys::ClassName)) return nullptr;

	string ClassName = Node[SceneKeys::ClassName].ToString();
	if (ClassName == "StaticMeshComp") { ClassName = "UStaticMeshComponent"; }

	// 💡 1. 덮어쓸 대상 결정 (기존 것이 있으면 쓰고, 없으면 새로 만든다)
	USceneComponent* Comp = ExistingComp;

	if (!Comp)
	{
		UObject* Obj = FObjectFactory::Get().Create(ClassName);
		if (!Obj || !Obj->IsA<USceneComponent>()) return nullptr;
		Comp = static_cast<USceneComponent*>(Obj);
		Owner->RegisterComponent(Comp);
	}

	// 💡 2. 프로퍼티 복원 (값 덮어쓰기)
	if (Node.hasKey(SceneKeys::Properties)) {
		auto PropsJSON = Node[SceneKeys::Properties];
		DeserializeProperties(Comp, PropsJSON);
	}
	Comp->MarkTransformDirty();

	// 💡 3. 자식 컴포넌트 병합 및 재귀 복원
	if (Node.hasKey(SceneKeys::Children))
	{
		// 부모 컴포넌트가 이미 가지고 있는 C++ 기본 자식들 목록
		auto ExistingChildren = Comp->GetChildren();

		// 짝을 찾았는지 체크하기 위한 배열 (중복 매칭 방지)
		std::vector<bool> Matched(ExistingChildren.size(), false);

		for (auto& ChildJSON : Node[SceneKeys::Children].ArrayRange())
		{
			if (!ChildJSON.hasKey(SceneKeys::ClassName)) continue;

			string ChildClassName = ChildJSON[SceneKeys::ClassName].ToString();
			if (ChildClassName == "StaticMeshComp") { ChildClassName = "UStaticMeshComponent"; }

			USceneComponent* TargetChild = nullptr;

			// 기존 자식들 중에서 클래스 이름이 같은 녀석을 찾는다
			for (size_t i = 0; i < ExistingChildren.size(); ++i)
			{
				if (!Matched[i] && ExistingChildren[i]->GetTypeInfo()->name == ChildClassName)
				{
					TargetChild = ExistingChildren[i];
					Matched[i] = true; // 짝을 찾았다고 표시
					break;
				}
			}

			// 재귀 호출 (TargetChild가 nullptr이면 내부에서 알아서 새로 만듦)
			USceneComponent* Child = DeserializeSceneComponentTree(ChildJSON, Owner, TargetChild);

			// 💡 기존에 없어서 이번에 "새로 만들어진" 자식이라면 Attach 해준다
			if (Child && !TargetChild)
			{
				Child->AttachToComponent(Comp);
			}
		}
	}

	return Comp;
}

void FSceneSaveManager::DeserializeNonSceneComponents(json::JSON& Node, AActor* Owner)
{
	if (Owner == nullptr)
	{
		return;
	}

	TArray<UActorComponent*> ExistingComponents;
	for (UActorComponent* Comp : Owner->GetComponents())
	{
		if (Comp != nullptr && Cast<USceneComponent>(Comp) == nullptr)
		{
			ExistingComponents.push_back(Comp);
		}
	}

	TArray<uint8> Matched;
	Matched.resize(ExistingComponents.size(), 0u);

	for (auto& ComponentJSON : Node.ArrayRange())
	{
		if (!ComponentJSON.hasKey(SceneKeys::ClassName))
		{
			continue;
		}

		string ClassName = ComponentJSON[SceneKeys::ClassName].ToString();
		UActorComponent* TargetComponent = nullptr;

		for (size_t Index = 0; Index < ExistingComponents.size(); ++Index)
		{
			if (Matched[Index] == 0u && ExistingComponents[Index]->GetTypeInfo()->name == ClassName)
			{
				TargetComponent = ExistingComponents[Index];
				Matched[Index] = 1u;
				break;
			}
		}

		if (TargetComponent == nullptr)
		{
			UObject* Obj = FObjectFactory::Get().Create(ClassName);
			TargetComponent = Obj ? Cast<UActorComponent>(Obj) : nullptr;
			if (TargetComponent == nullptr)
			{
				UObjectManager::Get().DestroyObject(Obj);
				continue;
			}

			Owner->RegisterComponent(TargetComponent);
		}

		if (ComponentJSON.hasKey(SceneKeys::Properties))
		{
			auto PropsJSON = ComponentJSON[SceneKeys::Properties];
			DeserializeProperties(TargetComponent, PropsJSON);
		}
	}
}

void FSceneSaveManager::DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON)
{
	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	// 💡 [1차 패스] StaticMesh나 Sprite 등 '뼈대'를 먼저 세팅해서 매테리얼 슬롯 빈칸들을 만듦!
	for (auto& Prop : Descriptors) {
		if (strcmp(Prop.Name, "StaticMesh") == 0 || strcmp(Prop.Name, "Sprite") == 0) {
			if (!PropsJSON.hasKey(Prop.Name)) continue;
			auto Value = PropsJSON[Prop.Name];
			DeserializePropertyValue(Prop, Value);
			Comp->PostEditProperty(Prop.Name);
		}
	}

	// 💡 뼈대가 세워지면서 슬롯이 생겼으므로 프로퍼티 리스트를 다시 가져옴 (이제 Material 0, Material 1 이 존재함)
	Descriptors.clear();
	Comp->GetEditableProperties(Descriptors);

	// 💡 [2차 패스] 매테리얼이나 위치, 스케일 등 나머지 값들을 덮어씌움
	for (auto& Prop : Descriptors) {
		if (strcmp(Prop.Name, "StaticMesh") == 0 || strcmp(Prop.Name, "Sprite") == 0) continue; // 1차에서 한 건 패스
		if (!PropsJSON.hasKey(Prop.Name)) continue;

		auto Value = PropsJSON[Prop.Name];
		DeserializePropertyValue(Prop, Value);
		Comp->PostEditProperty(Prop.Name);
	}
}
void FSceneSaveManager::DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value)
{
	switch (Prop.Type) {
	case EPropertyType::Bool:
		*static_cast<bool*>(Prop.ValuePtr) = Value.ToBool();
		break;

	case EPropertyType::Int:
		*static_cast<int32*>(Prop.ValuePtr) = Value.ToInt();
		break;

	case EPropertyType::Float:
		*static_cast<float*>(Prop.ValuePtr) = static_cast<float>(Value.ToFloat());
		break;

	case EPropertyType::Vec3: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 3) v[i] = static_cast<float>(elem.ToFloat());
			i++;
		}
		break;
	}
	case EPropertyType::Vec4:
	case EPropertyType::LinearColor: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 4) v[i] = static_cast<float>(elem.ToFloat());
			i++;
		}
		break;
	}
	case EPropertyType::String:
		*static_cast<FString*>(Prop.ValuePtr) = Value.ToString();
		break;

	case EPropertyType::Name:
		*static_cast<FName*>(Prop.ValuePtr) = FName(Value.ToString());
		break;

	default:
		break;
	}
}

void FSceneSaveManager::DeserializeCameraState(json::JSON& root, FEditorCameraState* OutCameraState /*= nullptr*/)
{
	using namespace json;
	// Perspective 카메라 상태 복원
	if (OutCameraState && root.hasKey(SceneKeys::PerspectiveCamera))
	{
		JSON Cam = root[SceneKeys::PerspectiveCamera];

		if (Cam.hasKey(SceneKeys::Location))
		{
			JSON Loc = Cam[SceneKeys::Location];
			OutCameraState->Location = FVector(
				static_cast<float>(Loc[0].ToFloat()),
				static_cast<float>(Loc[1].ToFloat()),
				static_cast<float>(Loc[2].ToFloat()));
		}
		if (Cam.hasKey(SceneKeys::Rotation))
		{
			JSON Rot = Cam[SceneKeys::Rotation];
			OutCameraState->Rotation = FRotator(
				static_cast<float>(Rot[0].ToFloat()),  // Pitch
				static_cast<float>(Rot[1].ToFloat()),  // Yaw
				static_cast<float>(Rot[2].ToFloat())); // Roll
		}
		
		// 수정: FOV, NearClip, FarClip이 배열([ ]) 형태로 들어오므로 0번째 인덱스로 접근
		if (Cam.hasKey(SceneKeys::FOV))
			OutCameraState->FOV = static_cast<float>(Cam[SceneKeys::FOV][0].ToFloat());
		if (Cam.hasKey(SceneKeys::NearClip))
			OutCameraState->NearClip = static_cast<float>(Cam[SceneKeys::NearClip][0].ToFloat());
		if (Cam.hasKey(SceneKeys::FarClip))
			OutCameraState->FarClip = static_cast<float>(Cam[SceneKeys::FarClip][0].ToFloat());

		OutCameraState->bValid = true;
	}
}

// ============================================================
// Utility
// ============================================================

string FSceneSaveManager::GetCurrentTimeStamp()
{
	std::time_t t = std::time(nullptr);
	std::tm tm{};
	localtime_s(&tm, &t);

	char buf[20];
	std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
	return buf;
}

TArray<FString> FSceneSaveManager::GetSceneFileList()
{
	TArray<FString> Result;
	std::wstring SceneDir = GetSceneDirectory();
	if (!std::filesystem::exists(SceneDir))
	{
		return Result;
	}

	for (auto& Entry : std::filesystem::directory_iterator(SceneDir))
	{
		if (Entry.is_regular_file() && Entry.path().extension() == SceneExtension)
		{
			Result.push_back(FPaths::ToUtf8(Entry.path().stem().wstring()));
		}
	}
	return Result;
}
