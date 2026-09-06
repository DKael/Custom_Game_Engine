#pragma once

#include "Object/Object.h"
#include "Core/PropertyTypes.h"

class AActor;
class UWorld;

class UActorComponent : public UObject
{
public:
    DECLARE_CLASS(UActorComponent, UObject)

    // Gameplay lifecycle is driven by AActor.
    virtual void BeginPlay();
    virtual void EndPlay() {};

    virtual void Activate();
    virtual void Deactivate();

    void ExecuteTick(float DeltaTime);
    void SetActive(bool bNewActive);
    inline void SetAutoActivate(bool bNewAutoActivate) { bAutoActivate = bNewAutoActivate; }
    inline void SetComponentTickEnabled(bool bEnabled) { bCanEverTick = bEnabled; }

    inline bool IsActive() { return bIsActive; }
    inline bool IsAutoActivate() { return bAutoActivate; }
    inline bool IsComponentTickEnabled() const { return bCanEverTick; }

    void SetOwner(AActor* Actor) { Owner = Actor; }
    AActor* GetOwner() const { return Owner; }

    // Expose editable properties to the editor details panel.
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    // Called after a property value is edited in the editor.
    void PostEditProperty(const char* PropertyName) override {}

    virtual void Serialize(FArchive& Ar) override;

    // Owner/parent-like relationships are managed by duplication callers.
    // CopyPropertiesFrom itself is defined on UObject.

    void SetTransient(bool bInTransient) { bTransient = bInTransient; }
    bool IsTransient() const { return bTransient; }

    void SetEditorOnly(bool bInEditorOnly) { bIsEditorOnly = bInEditorOnly; }
    bool IsEditorOnly() const { return bIsEditorOnly; }

    void SetHiddenInEditor(bool bInHidden) { bHiddenInEditor = bInHidden; }
    bool IsHiddenInEditor() const { return bHiddenInEditor; }

    // World registration lifecycle.
    // Guard checks (owner/world/registered-state) are centralized here.
    void OnRegister();
    void OnUnregister();
    bool IsRegistered() const { return bRegistered; }

protected:
    virtual void TickComponent(float DeltaTime) {}
    virtual void RegisterComponentWithWorld(UWorld& World) {}
    virtual void UnregisterComponentFromWorld(UWorld& World) {}
    bool TryGetOwnerWorld(UWorld*& OutWorld) const;

protected:
    AActor* Owner = nullptr;
    bool bRegistered = false;

protected:
    bool bIsActive = true;
    bool bAutoActivate = true;
    bool bCanEverTick = true;
    bool bTransient = false;      // Runtime-only component, excluded from serialization.
    bool bIsEditorOnly = false;   // Editor-only component, excluded from PIE/game rendering.
    bool bHiddenInEditor = false; // Hidden in the editor component list.
};
