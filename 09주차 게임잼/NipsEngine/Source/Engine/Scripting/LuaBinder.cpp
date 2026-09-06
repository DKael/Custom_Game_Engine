#include "Core/EnginePCH.h"
#include "Engine/Scripting/LuaBinder.h"

#include "Engine/Component/ActorComponent.h"
#include "Engine/Component/CameraComponent.h"
#include "Engine/Component/SceneComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Core/ActorTags.h"
#include "Engine/Core/CollisionTypes.h"
#include "Engine/Core/Paths.h"
#include "Engine/Core/Logging/Timer.h"
#include "Engine/Core/ResourceManager.h"
#include "Engine/GameFramework/Actor.h"
#include "Engine/GameFramework/Pawn.h"
#include "Engine/GameFramework/World.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/UI/UIElement.h"
#include "Engine/UI/UIManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace
{
    bool bUIMode = false;

    struct FDriftSalvageStats
    {
        int32 Health = 5;
        int32 Money = 0;
        float Weight = 0.0f;
    };

    struct FDriftSalvageCargoValue
    {
        float Weight = 0.0f;
        int32 Money = 0;
    };

    constexpr int32 DriftSalvageMaxHealth = 5;
    constexpr float DriftSalvageWeightCapacity = 150.0f;
    FDriftSalvageStats DriftSalvageStats;
    bool bDriftSalvageGameOverRequested = false;

    bool TryGetDriftSalvageCargoValue(const FString& ActorTag, FDriftSalvageCargoValue& OutValue)
    {
        if (ActorTag == ActorTags::Trash)
        {
            OutValue = { 10.0f, 1 };
            return true;
        }

        if (ActorTag == ActorTags::Resource)
        {
            OutValue = { 10.0f, 5 };
            return true;
        }

        if (ActorTag == ActorTags::Recyclable)
        {
            OutValue = { 5.0f, 5 };
            return true;
        }

        if (ActorTag == ActorTags::Premium)
        {
            OutValue = { 3.0f, 20 };
            return true;
        }

        return false;
    }

    // A valid actor can still be pending destroy; call sites choose strictness.
    bool IsValidActorObject(AActor* Actor)
    {
        return Actor && UObject::IsValid(Actor);
    }

    // For gameplay callbacks/bindings, pending-destroy actors are treated as unusable.
    bool IsUsableActor(AActor* Actor)
    {
        return IsValidActorObject(Actor) && !Actor->IsPendingDestroy();
    }

    bool IsUsableComponent(UActorComponent* Component)
    {
        return Component && UObject::IsValid(Component);
    }

    FString SafeObjectName(UObject* Object)
    {
        return (Object && UObject::IsValid(Object))
            ? static_cast<FString>(Object->GetName())
            : FString();
    }

    bool ResolveLuaWritablePath(const FString& RelativePath, std::filesystem::path& OutPath)
    {
        if (RelativePath.empty())
        {
            return false;
        }

        std::filesystem::path Input(FPaths::ToWide(RelativePath));
        if (Input.is_absolute())
        {
            return false;
        }

        std::filesystem::path Root(FPaths::RootDir());
        std::filesystem::path Target = (Root / Input).lexically_normal();
        std::filesystem::path Relative = Target.lexically_relative(Root);

        if (Relative.empty())
        {
            return false;
        }

        for (const std::filesystem::path& Part : Relative)
        {
            if (Part == L"..")
            {
                return false;
            }
        }

        OutPath = Target;
        return true;
    }

    // Type-name lookup used by both GetComponent and FindComponentByClass.
    UActorComponent* FindActorComponentByType(AActor* Actor, const FString& TypeName)
    {
        if (!IsUsableActor(Actor))
        {
            return nullptr;
        }

        const FString RequestedType = TypeName;
        const FString PrefixedType = RequestedType.rfind("U", 0) == 0
            ? RequestedType
            : ("U" + RequestedType);

        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (!IsUsableComponent(Component))
            {
                continue;
            }

            const FString ComponentType = Component->GetTypeInfo()->name;
            if (ComponentType == RequestedType || ComponentType == PrefixedType)
            {
                return Component;
            }
        }

        return nullptr;
    }

    void BindMathTypes(sol::state& Lua)
    {
        Lua.new_usertype<FVector>(
            "Vec3",
            sol::constructors<FVector(), FVector(float, float, float)>(),
            "X", &FVector::X,
            "Y", &FVector::Y,
            "Z", &FVector::Z);

        Lua.new_usertype<FHitResult>(
            "HitInfo",
            "Distance", &FHitResult::Distance,
            "Location", &FHitResult::Location,
            "Normal", &FHitResult::Normal,
            "FaceIndex", &FHitResult::FaceIndex,
            "IsValid", [](const FHitResult* Hit)
            {
                return Hit && Hit->IsValid();
            });
    }

    void BindComponentType(sol::state& Lua)
    {
        Lua.new_usertype<UActorComponent>(
            "Component",
            "GetName", [](UActorComponent* Component) -> FString
            {
                return SafeObjectName(Component);
            },
            "GetTypeName", [](UActorComponent* Component) -> FString
            {
                return IsUsableComponent(Component)
                    ? Component->GetTypeInfo()->name
                    : FString();
            },
            "GetOwner", [](UActorComponent* Component) -> AActor*
            {
                if (!IsUsableComponent(Component))
                {
                    return nullptr;
                }

                AActor* Owner = Component->GetOwner();
                return IsUsableActor(Owner) ? Owner : nullptr;
            },
            "SetActive", [](UActorComponent* Component, bool bEnabled)
            {
                if (IsUsableComponent(Component))
                {
                    Component->SetActive(bEnabled);
                }
            },
            "IsActive", [](UActorComponent* Component)
            {
                return IsUsableComponent(Component) && Component->IsActive();
            });
    }

    void BindSceneComponentType(sol::state& Lua)
    {
        Lua.new_usertype<USceneComponent>(
            "SceneComponent",
            sol::base_classes, sol::bases<UActorComponent>(),
            "GetParent", [](USceneComponent* Component) -> USceneComponent*
            {
                return Component ? Component->GetParent() : nullptr;
            },
            "GetWorldLocation", [](USceneComponent* Component)
            {
                return Component ? Component->GetWorldLocation() : FVector::ZeroVector;
            },
            "SetWorldLocation", [](USceneComponent* Component, float X, float Y, float Z)
            {
                if (Component)
                {
                    Component->SetWorldLocation(FVector(X, Y, Z));
                }
            },
            "AddWorldOffset", [](USceneComponent* Component, float X, float Y, float Z)
            {
                if (Component)
                {
                    Component->AddWorldOffset(FVector(X, Y, Z));
                }
            },
            "GetRelativeLocation", [](USceneComponent* Component)
            {
                return Component ? Component->GetRelativeLocation() : FVector::ZeroVector;
            },
            "SetRelativeLocation", [](USceneComponent* Component, float X, float Y, float Z)
            {
                if (Component)
                {
                    Component->SetRelativeLocation(FVector(X, Y, Z));
                }
            },
            "GetWorldRotation", [](USceneComponent* Component)
            {
                return Component ? Component->GetWorldTransform().GetRotation().Euler() : FVector::ZeroVector;
            },
            "GetRelativeRotation", [](USceneComponent* Component)
            {
                return Component ? Component->GetRelativeRotation() : FVector::ZeroVector;
            },
            "SetRelativeRotation", [](USceneComponent* Component, float X, float Y, float Z)
            {
                if (Component)
                {
                    Component->SetRelativeRotation(FVector(X, Y, Z));
                }
            },
            "GetForwardVector", [](USceneComponent* Component)
            {
                return Component ? Component->GetForwardVector() : FVector(1.0f, 0.0f, 0.0f);
            },
            "GetRightVector", [](USceneComponent* Component)
            {
                return Component ? Component->GetRightVector() : FVector(0.0f, 1.0f, 0.0f);
            },
            "GetUpVector", [](USceneComponent* Component)
            {
                return Component ? Component->GetUpVector() : FVector(0.0f, 0.0f, 1.0f);
            },
            "Move", [](USceneComponent* Component, float X, float Y, float Z)
            {
                if (Component)
                {
                    Component->Move(FVector(X, Y, Z));
                }
            },
            "MoveLocal", [](USceneComponent* Component, float X, float Y, float Z)
            {
                if (Component)
                {
                    Component->MoveLocal(FVector(X, Y, Z));
                }
            },
            "Rotate", [](USceneComponent* Component, float DeltaYaw, float DeltaPitch)
            {
                if (Component)
                {
                    Component->Rotate(DeltaYaw, DeltaPitch);
                }
            });
    }

    void BindStaticMeshComponentType(sol::state& Lua)
    {
        Lua.new_usertype<UStaticMeshComponent>(
            "StaticMeshComponent",
            sol::base_classes, sol::bases<USceneComponent, UActorComponent>());
    }

    void BindCameraComponentType(sol::state& Lua)
    {
        Lua.new_usertype<UCameraComponent>(
            "CameraComponent",
            sol::base_classes, sol::bases<USceneComponent, UActorComponent>(),
            "LookAt", [](UCameraComponent* Component, float X, float Y, float Z)
            {
                if (Component)
                {
                    Component->LookAt(FVector(X, Y, Z));
                }
            },
            "AddYawInput", [](UCameraComponent* Component, float DeltaYaw)
            {
                if (Component)
                {
                    Component->AddYawInput(DeltaYaw);
                }
            },
            "AddPitchInput", [](UCameraComponent* Component, float DeltaPitch)
            {
                if (Component)
                {
                    Component->AddPitchInput(DeltaPitch);
                }
            });
    }

    // mode 문자열 → FUICreateParams 변환 헬퍼
    FUICreateParams ParseUICreateParams(const std::string& Mode)
    {
        if (Mode == "FullRelative")   return FUICreateParams::FullRelative();
        if (Mode == "ParentRelative") return FUICreateParams::ParentRelative();
        if (Mode == "RelativePos")    return FUICreateParams::RelativePos();
        if (Mode == "Centered")       return FUICreateParams::Centered();
        return {};
    }

    // sol::object 에서 FUIElement* 추출 (nil·비 userdata 는 nullptr 반환)
    FUIElement* ExtractUIElement(sol::object Obj)
    {
        if (!Obj.valid() || Obj.get_type() != sol::type::userdata) return nullptr;
        if (auto* p = Obj.as<sol::optional<FUIImage*>>().value_or(nullptr))       return p;
        if (auto* p = Obj.as<sol::optional<FUIText*>>().value_or(nullptr))        return p;
        if (auto* p = Obj.as<sol::optional<FUIProgressBar*>>().value_or(nullptr)) return p;
        return nullptr;
    }

    void BindUITypes(sol::state& Lua)
    {
        // --- FUIElement 베이스 ---
        // SetVisible/Position/Size/Interactable + hover delegate 등록
        Lua.new_usertype<FUIElement>(
            "UIElement",
            "SetVisible",      &FUIElement::SetVisible,
            "IsVisible",       &FUIElement::IsVisible,
            "GetPosition",     [](FUIElement* E) -> std::tuple<float, float>
            {
                return E ? std::make_tuple(E->LocalPosition.X, E->LocalPosition.Y)
                         : std::make_tuple(0.f, 0.f);
            },
            "SetPosition",     [](FUIElement* E, float X, float Y)
            {
                if (E) E->LocalPosition = FVector2(X, Y);
            },
            "GetSize",         [](FUIElement* E) -> std::tuple<float, float>
            {
                return E ? std::make_tuple(E->Size.X, E->Size.Y)
                         : std::make_tuple(0.f, 0.f);
            },
            "SetSize",         [](FUIElement* E, float W, float H)
            {
                if (E) E->Size = FVector2(W, H);
            },
            "GetRotation",     [](FUIElement* E) -> float
            {
                return E ? E->RotationDegrees : 0.f;
            },
            "SetRotation",     [](FUIElement* E, float Degrees)
            {
                if (E) E->RotationDegrees = Degrees;
            },
            "SetInteractable", [](FUIElement* E, bool bEnabled)
            {
                if (E) E->bInteractable = bEnabled;
            },
            // element:OnHoverEnter(function() ... end)  — 누적 등록
            "OnHoverEnter",    [](FUIElement* E, sol::protected_function Cb)
            {
                if (!E) return;
                E->OnHoverEnter.Add([cb = std::move(Cb)]() mutable { cb(); });
            },
            "OnHoverExit",     [](FUIElement* E, sol::protected_function Cb)
            {
                if (!E) return;
                E->OnHoverExit.Add([cb = std::move(Cb)]() mutable { cb(); });
            },
            // element:OnClick(function() ... end)  — 좌클릭 Down 프레임에 발생
            "OnClick",         [](FUIElement* E, sol::protected_function Cb)
            {
                if (!E) return;
                E->OnClick.Add([cb = std::move(Cb)]() mutable { cb(); });
            }
        );

        // --- FUIImage ---
        Lua.new_usertype<FUIImage>(
            "UIImage",
            sol::base_classes, sol::bases<FUIElement>(),
            "SetColor",   [](FUIImage* I, float R, float G, float B, float A)
            {
                if (I) I->TintColor = FVector4(R, G, B, A);
            },
            "SetTexture", [](FUIImage* I, const FString& Name)
            {
                if (I) I->Texture = FResourceManager::Get().LoadTexture(Name);
            },
            // 스프라이트 시트 UV 크롭: image:SetUV(minX, minY, maxX, maxY)
            "SetUV",      [](FUIImage* I, float MinX, float MinY, float MaxX, float MaxY)
            {
                if (I) I->SetUV({ MinX, MinY }, { MaxX, MaxY });
            }
        );

        // --- FUIText ---
        Lua.new_usertype<FUIText>(
            "UIText",
            sol::base_classes, sol::bases<FUIElement>(),
            "SetText",    [](FUIText* T, const FString& Text)
            {
                if (T) T->Text = Text;
            },
            "GetText",    [](FUIText* T) -> FString
            {
                return T ? T->Text : FString();
            },
            "SetColor",   [](FUIText* T, float R, float G, float B, float A)
            {
                if (T) T->Color = FVector4(R, G, B, A);
            },
            "SetFontSize",[](FUIText* T, float Size)
            {
                if (T) T->FontSize = Size;
            }
        );

        // --- FUIProgressBar ---
        Lua.new_usertype<FUIProgressBar>(
            "UIProgressBar",
            sol::base_classes, sol::bases<FUIElement>(),
            "SetValue",     [](FUIProgressBar* B, float V)
            {
                if (B) B->SetValue(V);
            },
            "GetValue",     [](FUIProgressBar* B) -> float
            {
                return B ? B->GetValue() : 0.f;
            },
            "SetFillColor", [](FUIProgressBar* B, float R, float G, float B2, float A)
            {
                if (B) B->FillColor = FVector4(R, G, B2, A);
            },
            "SetBgColor",   [](FUIProgressBar* B, float R, float G, float B2, float A)
            {
                if (B) B->BgColor = FVector4(R, G, B2, A);
            }
        );

        // --- UIManager 글로벌 테이블 ---
        // 호출 형식:
        //   UIManager.CreateImage(parent, x, y, w, h, textureName, mode)
        //   UIManager.CreateText(parent, x, y, w, h, text, fontSize, mode)
        //   UIManager.CreateProgressBar(parent, x, y, w, h, mode)
        //   UIManager.DestroyElement(element)
        //
        // parent / textureName / mode 는 모두 nil 가능 (옵션)
        // mode 문자열: "FullRelative", "ParentRelative", "RelativePos", "Centered"
        sol::table UIManagerTable = Lua.create_table();

        UIManagerTable.set_function("CreateImage",
            [](sol::object ParentObj, float X, float Y, float W, float H,
               sol::object TexObj, sol::object ModeObj) -> FUIImage*
            {
                FUIElement* Parent = ExtractUIElement(ParentObj);

                UTexture* Tex = nullptr;
                if (TexObj.get_type() == sol::type::string)
                    Tex = FResourceManager::Get().LoadTexture(TexObj.as<FString>());

                FUICreateParams Params;
                if (ModeObj.get_type() == sol::type::string)
                    Params = ParseUICreateParams(ModeObj.as<std::string>());

                // 텍스처 없이 색상도 지정 안 하면 완전 투명 (패널 용도)
                const FVector4 Color = (Tex != nullptr) ? FVector4(1, 1, 1, 1)
                                                        : FVector4(0, 0, 0, 0);
                return FUIManager::Get().CreateImage(
                    Parent, FVector2(X, Y), FVector2(W, H), Tex, Color, Params);
            });

        UIManagerTable.set_function("CreateText",
            [](sol::object ParentObj, float X, float Y, float W, float H,
               const FString& Text, sol::object FontSizeObj, sol::object ModeObj) -> FUIText*
            {
                FUIElement* Parent = ExtractUIElement(ParentObj);

                float FontSize = 16.f;
                if (FontSizeObj.get_type() == sol::type::number)
                    FontSize = FontSizeObj.as<float>();

                FUICreateParams Params;
                if (ModeObj.get_type() == sol::type::string)
                    Params = ParseUICreateParams(ModeObj.as<std::string>());

                return FUIManager::Get().CreateText(
                    Parent, FVector2(X, Y), FVector2(W, H), Text, FontSize, {0, 0, 0, 1}, Params);
            });

        UIManagerTable.set_function("CreateProgressBar",
            [](sol::object ParentObj, float X, float Y, float W, float H,
               sol::object ModeObj) -> FUIProgressBar*
            {
                FUIElement* Parent = ExtractUIElement(ParentObj);

                FUICreateParams Params;
                if (ModeObj.get_type() == sol::type::string)
                    Params = ParseUICreateParams(ModeObj.as<std::string>());

                return FUIManager::Get().CreateProgressBar(
                    Parent, FVector2(X, Y), FVector2(W, H),
                    {0.8f, 0.1f, 0.1f, 1.f}, {0.2f, 0.2f, 0.2f, 1.f}, Params);
            });

        UIManagerTable.set_function("DestroyElement",
            [](sol::object Obj)
            {
                FUIElement* Element = ExtractUIElement(Obj);
                if (Element) FUIManager::Get().DestroyElement(Element);
            });

        Lua["UIManager"] = UIManagerTable;
    }

    void BindActorType(sol::state& Lua)
    {
        Lua.new_usertype<AActor>(
            "Actor",
            "GetName", [](AActor* Actor) -> FString
            {
                return SafeObjectName(Actor);
            },
            "GetTag", [](AActor* Actor) -> FString
            {
                return IsUsableActor(Actor) ? Actor->GetTag() : FString();
            },
            "GetPosition", [](AActor* Actor)
            {
                return IsUsableActor(Actor)
                    ? Actor->GetActorLocation()
                    : FVector::ZeroVector;
            },
            "SetPosition", [](AActor* Actor, float X, float Y, float Z)
            {
                if (IsUsableActor(Actor))
                {
                    Actor->SetActorLocation(FVector(X, Y, Z));
                }
            },
            "AddPosition", [](AActor* Actor, float X, float Y, float Z)
            {
                if (IsUsableActor(Actor))
                {
                    Actor->AddActorWorldOffset(FVector(X, Y, Z));
                }
            },
            "GetRotation", [](AActor* Actor)
            {
                return IsUsableActor(Actor)
                    ? Actor->GetActorRotation()
                    : FVector::ZeroVector;
            },
            "SetRotation", [](AActor* Actor, float X, float Y, float Z)
            {
                if (IsUsableActor(Actor))
                {
                    Actor->SetActorRotation(FVector(X, Y, Z));
                }
            },
            "GetScale", [](AActor* Actor)
            {
                return IsUsableActor(Actor)
                    ? Actor->GetActorScale()
                    : FVector(1.0f, 1.0f, 1.0f);
            },
            "SetScale", [](AActor* Actor, float X, float Y, float Z)
            {
                if (IsUsableActor(Actor))
                {
                    Actor->SetActorScale(FVector(X, Y, Z));
                }
            },
            "GetForwardVector", [](AActor* Actor)
            {
                if (APawn* Pawn = Cast<APawn>(Actor))
                {
                    return Pawn->GetForwardVector();
                }

                return IsUsableActor(Actor)
                    ? Actor->GetActorForward()
                    : FVector(0.0f, 0.0f, 1.0f);
            },
            "SetActive", [](AActor* Actor, bool bEnabled)
            {
                if (IsUsableActor(Actor))
                {
                    Actor->SetActive(bEnabled);
                }
            },
            "SetVisible", [](AActor* Actor, bool bVisible)
            {
                if (IsUsableActor(Actor))
                {
                    Actor->SetVisible(bVisible);
                }
            },
            "IsVisible", [](AActor* Actor)
            {
                return IsUsableActor(Actor) && Actor->IsVisible();
            },
            "Destroy", [](AActor* Actor)
            {
                if (IsUsableActor(Actor))
                {
                    Actor->Destroy();
                }
            },
            "GetComponent", [](AActor* Actor, const FString& TypeName) -> UActorComponent*
            {
                return FindActorComponentByType(Actor, TypeName);
            },
            "FindComponentByClass", [](AActor* Actor, const FString& TypeName) -> UActorComponent*
            {
                return FindActorComponentByType(Actor, TypeName);
            },
            "GetRootComponent", [](AActor* Actor) -> USceneComponent*
            {
                if (!IsUsableActor(Actor))
                {
                    return nullptr;
                }

                return Actor->GetRootComponent();
            },
            "IsPendingDestroy", [](AActor* Actor)
            {
                return IsValidActorObject(Actor) && Actor->IsPendingDestroy();
            },
            "IsValid", [](AActor* Actor)
            {
                return IsUsableActor(Actor);
            });
    }

    void BindPawnType(sol::state& Lua)
    {
        Lua.new_usertype<APawn>(
            "Pawn",
            sol::base_classes, sol::bases<AActor>(),
            "GetCharacterComponent", [](APawn* Pawn) -> UStaticMeshComponent*
            {
                return Pawn ? Pawn->GetCharacterComponent() : nullptr;
            },
            "GetCameraComponent", [](APawn* Pawn) -> UCameraComponent*
            {
                return Pawn ? Pawn->GetCameraComponent() : nullptr;
            },
            "GetForwardVector", [](APawn* Pawn)
            {
                return Pawn ? Pawn->GetForwardVector() : FVector(1.0f, 0.0f, 0.0f);
            },
            "GetRightVector", [](APawn* Pawn)
            {
                return Pawn ? Pawn->GetRightVector() : FVector(0.0f, 1.0f, 0.0f);
            },
            "GetUpVector", [](APawn* Pawn)
            {
                return Pawn ? Pawn->GetUpVector() : FVector(0.0f, 0.0f, 1.0f);
            },
            "UpdateBoatMovement", [](APawn* Pawn,
                                     float DeltaTime,
                                     float ThrottleInput,
                                     float SteerInput,
                                     float Mass,
                                     float ForwardAccel,
                                     float ReverseAccel,
                                     float BrakeAccel,
                                     float LinearDrag,
                                     float TurnAccel,
                                     float TurnDrag,
                                     float MaxForwardSpeed,
                                     float MaxReverseSpeed,
                                     float MaxYawSpeed,
                                     float MinSteerAuthority,
                                     float SpeedEpsilon)
            {
                if (Pawn)
                {
                    Pawn->UpdateBoatMovement(
                        DeltaTime,
                        ThrottleInput,
                        SteerInput,
                        Mass,
                        ForwardAccel,
                        ReverseAccel,
                        BrakeAccel,
                        LinearDrag,
                        TurnAccel,
                        TurnDrag,
                        MaxForwardSpeed,
                        MaxReverseSpeed,
                        MaxYawSpeed,
                        MinSteerAuthority,
                        SpeedEpsilon);
                }
            },
            "GetBoatForwardSpeed", [](APawn* Pawn)
            {
                return Pawn ? Pawn->GetBoatForwardSpeed() : 0.0f;
            },
            "GetBoatYawSpeed", [](APawn* Pawn)
            {
                return Pawn ? Pawn->GetBoatYawSpeed() : 0.0f;
            });
    }
}

void LuaBinder::SetUIMode(bool bEnabled)
{
    bUIMode = bEnabled;
}

bool LuaBinder::IsUIMode()
{
    return bUIMode;
}

void LuaBinder::ResetDriftSalvageStats()
{
    DriftSalvageStats.Health = DriftSalvageMaxHealth;
    DriftSalvageStats.Money = 0;
    DriftSalvageStats.Weight = 0.0f;
    bDriftSalvageGameOverRequested = false;
}

void LuaBinder::RequestDriftSalvageGameOver()
{
    // 기존 HUD의 체력 기반 Game Over 조건과 즉시 전환 요청을 함께 맞춘다
    DriftSalvageStats.Health = 0;
    bDriftSalvageGameOverRequested = true;
}

bool LuaBinder::ConsumeDriftSalvageGameOverRequest()
{
    // HUD가 한 번만 반응하도록 요청을 읽은 뒤 바로 비운다
    const bool bRequested = bDriftSalvageGameOverRequested;
    bDriftSalvageGameOverRequested = false;
    return bRequested;
}

void LuaBinder::ApplyDriftSalvageDamage(int32 Damage)
{
    if (Damage <= 0)
    {
        return;
    }

    DriftSalvageStats.Health = std::max(0, DriftSalvageStats.Health - Damage);
}

void LuaBinder::ApplyDriftSalvagePickup(const FString& ActorTag)
{
    TryApplyDriftSalvagePickup(ActorTag);
}

bool LuaBinder::TryApplyDriftSalvagePickup(const FString& ActorTag)
{
    if (ActorTag == ActorTags::Hazard)
    {
        ApplyDriftSalvageDamage(2);
        return true;
    }

    FDriftSalvageCargoValue CargoValue;
    if (!TryGetDriftSalvageCargoValue(ActorTag, CargoValue))
    {
        return false;
    }

    if (DriftSalvageStats.Weight + CargoValue.Weight > DriftSalvageWeightCapacity)
    {
        return false;
    }

    DriftSalvageStats.Weight += CargoValue.Weight;
    DriftSalvageStats.Money += CargoValue.Money;
    return true;
}

float LuaBinder::GetDriftSalvagePickupWeight(const FString& ActorTag)
{
    FDriftSalvageCargoValue CargoValue;
    return TryGetDriftSalvageCargoValue(ActorTag, CargoValue) ? CargoValue.Weight : 0.0f;
}

bool LuaBinder::CanApplyDriftSalvagePickup(const FString& ActorTag, float ReservedWeight)
{
    if (ActorTag == ActorTags::Hazard)
    {
        return true;
    }

    FDriftSalvageCargoValue CargoValue;
    if (!TryGetDriftSalvageCargoValue(ActorTag, CargoValue))
    {
        return false;
    }

    return DriftSalvageStats.Weight + std::max(0.0f, ReservedWeight) + CargoValue.Weight <= DriftSalvageWeightCapacity;
}

int32 LuaBinder::GetDriftSalvageHealth()
{
    return DriftSalvageStats.Health;
}

int32 LuaBinder::GetDriftSalvageMoney()
{
    return DriftSalvageStats.Money;
}

float LuaBinder::GetDriftSalvageWeight()
{
    return DriftSalvageStats.Weight;
}

float LuaBinder::GetDriftSalvageWeightCapacity()
{
    return DriftSalvageWeightCapacity;
}

void LuaBinder::BindEngineTypes(sol::state& Lua)
{
    // Keep registration split by domain so additions stay localized.
    BindMathTypes(Lua);
    BindComponentType(Lua);
    BindSceneComponentType(Lua);
    BindStaticMeshComponentType(Lua);
    BindCameraComponentType(Lua);
    BindActorType(Lua);
    BindPawnType(Lua);
    BindUITypes(Lua);
}

void LuaBinder::BindGlobalFunctions(sol::state& Lua)
{
    sol::table InputTable = Lua.create_table();
    auto ToVirtualKey = [](const std::string& Key) -> int
    {
        if (Key.empty())
        {
            return 0;
        }

        if (Key == "Left" || Key == "ArrowLeft")
        {
            return VK_LEFT;
        }

        if (Key == "Right" || Key == "ArrowRight")
        {
            return VK_RIGHT;
        }

        unsigned char Ch = static_cast<unsigned char>(Key[0]);
        if (Ch >= 'a' && Ch <= 'z')
        {
            Ch = static_cast<unsigned char>(Ch - 'a' + 'A');
        }

        return static_cast<int>(Ch);
    };

    InputTable.set_function("GetKeyDown", [ToVirtualKey](const std::string& Key)
    {
        const int VK = ToVirtualKey(Key);
        return VK > 0 && InputSystem::Get().GetKeyDown(VK);
    });

    InputTable.set_function("GetKey", [ToVirtualKey](const std::string& Key)
    {
        const int VK = ToVirtualKey(Key);
        return VK > 0 && InputSystem::Get().GetKey(VK);
    });

    InputTable.set_function("GetKeyUp", [ToVirtualKey](const std::string& Key)
    {
        const int VK = ToVirtualKey(Key);
        return VK > 0 && InputSystem::Get().GetKeyUp(VK);
    });

    Lua["Input"] = InputTable;

    Lua.set_function("SetUIMode", [](bool bEnabled)
    {
        LuaBinder::SetUIMode(bEnabled);
    });

    Lua.set_function("IsUIMode", []()
    {
        return LuaBinder::IsUIMode();
    });

    Lua.set_function("FindActorByTag", [](const FString& Tag) -> AActor*
    {
        UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
        if (!World)
        {
            return nullptr;
        }

        for (AActor* Actor : World->GetActors())
        {
            if (IsUsableActor(Actor) && Actor->CompareTag(Tag))
            {
                return Actor;
            }
        }

        return nullptr;
    });
    
    Lua.set_function("FindActorsByTag", [](sol::this_state ts, const FString& Tag) -> sol::table
    {
        sol::state_view L(ts);
        sol::table Result = L.create_table();

        UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
        if (!World)
        {
            return Result;
        }

        int32 Index = 1;
        for (AActor* Actor : World->GetActors())
        {
            if (IsUsableActor(Actor) && Actor->CompareTag(Tag))
            {
                Result[Index++] = Actor;
            }
        }

        return Result;
    });

    Lua.set_function("IsActorPushed", [](AActor* Actor)
    {
        if (!IsUsableActor(Actor))
        {
            return false;
        }

        UWorld* World = Actor->GetFocusedWorld();
        return World && World->GetExplosionSystem().IsActorPushed(Actor);
    });

    Lua.set_function("RequestGameRestart", []()
    {
        if (GEngine)
        {
            GEngine->RequestGameRestart();
        }
    });

    Lua.set_function("ResetDriftSalvageStats", []()
    {
        LuaBinder::ResetDriftSalvageStats();
    });

    Lua.set_function("ConsumeDriftSalvageGameOverRequest", []()
    {
        return LuaBinder::ConsumeDriftSalvageGameOverRequest();
    });

    Lua.set_function("GetDriftSalvageHealth", []()
    {
        return LuaBinder::GetDriftSalvageHealth();
    });

    Lua.set_function("GetDriftSalvageMoney", []()
    {
        return LuaBinder::GetDriftSalvageMoney();
    });

    Lua.set_function("GetDriftSalvageWeight", []()
    {
        return LuaBinder::GetDriftSalvageWeight();
    });

    Lua.set_function("GetDriftSalvageWeightCapacity", []()
    {
        return LuaBinder::GetDriftSalvageWeightCapacity();
    });

    Lua.set_function("ReadTextFile", [](const FString& RelativePath) -> FString
    {
        std::filesystem::path FilePath;
        if (!ResolveLuaWritablePath(RelativePath, FilePath))
        {
            return FString();
        }

        std::ifstream File(FilePath, std::ios::in | std::ios::binary);
        if (!File.is_open())
        {
            return FString();
        }

        return FString((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    });

    Lua.set_function("WriteTextFile", [](const FString& RelativePath, const FString& Content) -> bool
    {
        std::filesystem::path FilePath;
        if (!ResolveLuaWritablePath(RelativePath, FilePath))
        {
            return false;
        }

        std::filesystem::create_directories(FilePath.parent_path());

        std::ofstream File(FilePath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!File.is_open())
        {
            return false;
        }

        File << Content;
        return File.good();
    });

    Lua.set_function("Log", [](const FString& Message)
    {
        UE_LOG("[Lua] %s\n", Message.c_str());
    });

    Lua.set_function("Warning", [](const FString& Message)
    {
        UE_LOG("[Lua Warning] %s\n", Message.c_str());
    });

    Lua.set_function("Error", [](const FString& Message)
    {
        UE_LOG("[Lua Error] %s\n", Message.c_str());
    });

    Lua.set_function("GetTimeSeconds", []() -> double
    {
        return (GEngine && GEngine->GetTimer()) ? GEngine->GetTimer()->GetTotalTime() : 0.0;
    });

    Lua.set_function("GetFrameCount", []() -> uint64
    {
        return GEngine ? GEngine->GetFrameCount() : 0;
    });
}
