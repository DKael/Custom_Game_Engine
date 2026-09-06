#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include "Core/CoreTypes.h"      // int32, uint8, …
#include "Core/Containers/Array.h"
#include "Reflection/FField.h"
#include "Math/Vector.h"    // FVector  (for sizeof in GetPropertySize)
#include "Math/Vector4.h"   // FVector4 (for sizeof in GetPropertySize)
#include "Math/Color.h"     // FColor

class UObject;
struct FArchive;
class UStruct;
class UScriptStruct;
class UEnum;
class UClass;

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입
// UPROPERTY() 매크로로 지정할 수 없는 타입: Material, SRV, CubeSRV, Lua
enum class EPropertyType : uint8_t
{
    Bool,
    Int,
    Float,
    Vec3,
    Vec4,
    String,
    Name,              // FName — 문자열 풀 기반 이름 (리소스 키 등)
    ObjectRef,         // UObject* 변수의 주소
    SceneComponentRef, // USceneComponent* 변수의 주소 (MovementComponent를 위한 Enum값
    Vec3Array,         // TArray<FVector>* - variable-length array of FVector
                       // 필요 시 Enum, Color 등 추가
	Enum,
    Struct,
	Color,

	Material, // TODO: 수정필요
	SRV,
    CubeSRV, // Shadow Cube Map Face SRV
};

enum class EPropertyUsageFlags : uint8_t
{
    None = 0,
    Editable = 1 << 0,
    Animatable = 1 << 1,
    Visible = 1 << 2,
    NonSerialized = 1 << 3,
};

constexpr EPropertyUsageFlags operator|(EPropertyUsageFlags Lhs, EPropertyUsageFlags Rhs)
{
    return static_cast<EPropertyUsageFlags>(
        static_cast<uint8_t>(Lhs) | static_cast<uint8_t>(Rhs));
}

constexpr bool HasPropertyUsage(EPropertyUsageFlags Value, EPropertyUsageFlags Flag)
{
    return (static_cast<uint8_t>(Value) & static_cast<uint8_t>(Flag)) != 0;
}

enum class EPropertyChangeType : uint8_t
{
    ValueSet,
    Interactive,
    Preview,
    Runtime,
};

struct FPropertyChangedEvent
{
    const char* PropertyName = nullptr;
    EPropertyChangeType ChangeType = EPropertyChangeType::ValueSet;
};

// SRV 정보
struct FSRVDisplayInfo
{
    float ImageWidth  = 256.f;
    float ImageHeight = 256.f;
    float UV0X = 0.f;
    float UV0Y = 0.f;
    float UV1X = 1.f;
    float UV1Y = 1.f;
};

// UObject가 노출하는 공통 프로퍼티 디스크립터
struct FPropertyDescriptor
{
    const char*   Name;
    EPropertyType Type;
    void*         ValuePtr;

    // float 범위 힌트 (DragFloat 등에서 사용)
    float Min	= 0.0f;
    float Max	= 0.0f;
    float Speed = 0.1f;

	// Enum Metadata
	const char* const* EnumNames = nullptr;
	uint32		 EnumCount  = 0;

    // 타입별 추가 메타데이터 일단 SRV 정보를 저장하기 위해서 사용
    void* ExtraData = nullptr;

    EPropertyUsageFlags UsageFlags = EPropertyUsageFlags::Editable;
    const UClass* ObjectType = nullptr;
    const char* DisplayName = nullptr;
};

enum class EPropertyAccess : uint8_t
{
    EditAnywhere,
    VisibleAnywhere,
};

/*
Reflection 시스템 계층구조

UObject
  └ UField
	 ├─ UStruct
	 │    ├─ UClass
	 │    └─ UScriptStruct
	 └─ UEnum

FField
  └─ FProperty
       ├─ FBoolProperty
       ├─ FIntProperty
       ├─ FFloatProperty
       ├─ FObjectProperty
       ├─ FStructProperty
       ├─ FEnumProperty
       └─ FArrayProperty
*/
class FProperty : public FField
{
public:
    FProperty(
        const char* InName,
        const char* InDisplayName,
        const char* InSerializeName,
        size_t InOffset,
        EPropertyAccess InAccess,
        float InMin = 0.0f,
        float InMax = 0.0f,
        float InSpeed = 0.1f,
        EPropertyUsageFlags InUsageFlags = EPropertyUsageFlags::None)
        : FField(InName)
        , DisplayName(InDisplayName)
        , SerializeName(InSerializeName)
        , Offset(InOffset)
        , Access(InAccess)
        , Min(InMin)
        , Max(InMax)
        , Speed(InSpeed)
        , UsageFlags(InUsageFlags)
    {
    }

    virtual ~FProperty() = default;

    const char* GetDisplayName() const { return DisplayName; }
    const char* GetSerializeName() const { return SerializeName; }
    const char* GetSerializeKey() const { return SerializeName ? SerializeName : (DisplayName ? DisplayName : GetName()); }
    size_t GetOffset() const { return Offset; }
    EPropertyAccess GetAccess() const { return Access; }
    float GetMin() const { return Min; }
    float GetMax() const { return Max; }
    float GetSpeed() const { return Speed; }
    EPropertyUsageFlags GetUsageFlags() const { return UsageFlags; }
    bool ShouldSerialize() const { return !HasPropertyUsage(UsageFlags, EPropertyUsageFlags::NonSerialized); }

    EPropertyUsageFlags GetDescriptorUsageFlags() const
    {
        const EPropertyUsageFlags BaseUsage =
            Access == EPropertyAccess::EditAnywhere
                ? (EPropertyUsageFlags::Visible | EPropertyUsageFlags::Editable)
                : EPropertyUsageFlags::Visible;
        return BaseUsage | UsageFlags;
    }

    void* ContainerPtrToValuePtr(void* Container) const
    {
        return static_cast<uint8*>(Container) + Offset;
    }

    const void* ContainerPtrToValuePtr(const void* Container) const
    {
        return static_cast<const uint8*>(Container) + Offset;
    }

    virtual EPropertyType GetType() const = 0;
    virtual const UClass* GetObjectType() const { return nullptr; }
    virtual bool ShouldCheckSerializeKey() const { return true; }
    virtual void AppendEditorDescriptor(void* Container, TArray<FPropertyDescriptor>& OutProps) const;
    virtual void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const = 0;

private:
    const char* DisplayName = nullptr;
    const char* SerializeName = nullptr;
    size_t Offset = 0;
    EPropertyAccess Access = EPropertyAccess::EditAnywhere;
    float Min = 0.0f;
    float Max = 0.0f;
    float Speed = 0.1f;
    EPropertyUsageFlags UsageFlags = EPropertyUsageFlags::None;
};

class FBoolProperty : public FProperty
{
public:
    using FProperty::FProperty;
    EPropertyType GetType() const override { return EPropertyType::Bool; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;
};

class FIntProperty : public FProperty
{
public:
    using FProperty::FProperty;
    EPropertyType GetType() const override { return EPropertyType::Int; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;
};

class FFloatProperty : public FProperty
{
public:
    using FProperty::FProperty;
    EPropertyType GetType() const override { return EPropertyType::Float; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;
};

class FVectorProperty : public FProperty
{
public:
    using FProperty::FProperty;
    EPropertyType GetType() const override { return EPropertyType::Vec3; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;
};

class FVector4Property : public FProperty
{
public:
    using FProperty::FProperty;
    EPropertyType GetType() const override { return EPropertyType::Vec4; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;
};

class FStringProperty : public FProperty
{
public:
    using FProperty::FProperty;
    EPropertyType GetType() const override { return EPropertyType::String; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;
};

class FNameProperty : public FProperty
{
public:
    using FProperty::FProperty;
    EPropertyType GetType() const override { return EPropertyType::Name; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;
};

class FColorProperty : public FProperty
{
public:
    using FProperty::FProperty;
    EPropertyType GetType() const override { return EPropertyType::Color; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;
};

class FObjectProperty : public FProperty
{
public:
    FObjectProperty(
        const char* InName,
        const char* InDisplayName,
        const char* InSerializeName,
        size_t InOffset,
        EPropertyAccess InAccess,
        const UClass* InObjectType,
        float InMin = 0.0f,
        float InMax = 0.0f,
        float InSpeed = 0.1f,
        EPropertyUsageFlags InUsageFlags = EPropertyUsageFlags::None)
        : FProperty(InName, InDisplayName, InSerializeName, InOffset, InAccess, InMin, InMax, InSpeed, InUsageFlags)
        , ObjectType(InObjectType)
    {
    }

    EPropertyType GetType() const override { return EPropertyType::ObjectRef; }
    const UClass* GetObjectType() const override { return ObjectType; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;

private:
    const UClass* ObjectType = nullptr;
};

class FSceneComponentProperty : public FProperty
{
public:
    using FProperty::FProperty;
    EPropertyType GetType() const override { return EPropertyType::SceneComponentRef; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;
};

class FStructProperty : public FProperty
{
public:
    FStructProperty(
        const char* InName,
        const char* InDisplayName,
        const char* InSerializeName,
        size_t InOffset,
        EPropertyAccess InAccess,
        const UScriptStruct* InStruct,
        float InMin = 0.0f,
        float InMax = 0.0f,
        float InSpeed = 0.1f,
        EPropertyUsageFlags InUsageFlags = EPropertyUsageFlags::None)
        : FProperty(InName, InDisplayName, InSerializeName, InOffset, InAccess, InMin, InMax, InSpeed, InUsageFlags)
        , Struct(InStruct)
    {
    }

    EPropertyType GetType() const override { return EPropertyType::Struct; }
    bool ShouldCheckSerializeKey() const override { return false; }
    void AppendEditorDescriptor(void* Container, TArray<FPropertyDescriptor>& OutProps) const override;
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;

private:
    const UScriptStruct* Struct = nullptr;
};

class FEnumProperty : public FProperty
{
public:
    FEnumProperty(
        const char* InName,
        const char* InDisplayName,
        const char* InSerializeName,
        size_t InOffset,
        EPropertyAccess InAccess,
        const UEnum* InEnum,
        float InMin = 0.0f,
        float InMax = 0.0f,
        float InSpeed = 0.1f,
        EPropertyUsageFlags InUsageFlags = EPropertyUsageFlags::None)
        : FProperty(InName, InDisplayName, InSerializeName, InOffset, InAccess, InMin, InMax, InSpeed, InUsageFlags)
        , Enum(InEnum)
    {
    }

    EPropertyType GetType() const override { return EPropertyType::Enum; }
    void AppendEditorDescriptor(void* Container, TArray<FPropertyDescriptor>& OutProps) const override;
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;

private:
    const UEnum* Enum = nullptr;
};

class FArrayProperty : public FProperty
{
public:
    using FProperty::FProperty;
    EPropertyType GetType() const override { return EPropertyType::Vec3Array; }
    void SerializeItem(FArchive& Ar, UObject* OwnerObject, void* Container) const override;
};

/** 각 프로퍼티의 Size 값을 반환합니다. 0을 반환하는 경우 특수 케이스입니다.
 * 이런 경우에는 CopyPropertiesFrom 함수 내에서 알아서 잘 처리해줄 수 있어야 합니다. 
 **/
inline size_t GetPropertySize(EPropertyType Type)
{
    switch (Type)
    {
    case EPropertyType::Bool:   return sizeof(bool);
    case EPropertyType::Int:    return sizeof(int32);
    case EPropertyType::Float:  return sizeof(float);
    case EPropertyType::Vec3:   return sizeof(FVector);
    case EPropertyType::Color:	return sizeof(FColor);
    case EPropertyType::Vec4:   return sizeof(FVector4);
    // String, Name 은 ValuePtr 기반 특수 처리
    case EPropertyType::String: return 0;
    case EPropertyType::Name:   return 0;
    case EPropertyType::ObjectRef: return sizeof(void*);
    case EPropertyType::Enum:   return sizeof(int32);
    // 포인터 — Duplicate 호출 측에서 직접 처리
    case EPropertyType::SceneComponentRef: return 0;
    default: return 0;
    }
}
