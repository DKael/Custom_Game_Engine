#pragma once

#include "AActor.h"
#include "GameFramework/Pawn.h"
#include "Core/Delegates/Delegate.h"
#include "Core/CollisionTypes.h"
#include "Generated/PrimitiveActors.generated.h"

class UTextRenderComponent;
class UDecalComponent;
class ULightComponent;
class UBillboardComponent;
class UHeightFogComponent;
class UBoxComponent;
class UProjectileMovementComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class USkeletalMeshComponent;

UCLASS()
class ACubeActor : public AActor
{
public:
	GENERATED_BODY()

	ACubeActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class ASphereActor : public AActor
{
public:
	GENERATED_BODY()

	ASphereActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class APlaneActor : public AActor
{
public:
	GENERATED_BODY()

	APlaneActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class AAttachTestActor : public AActor
{
public:
	GENERATED_BODY()

	AAttachTestActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class ASceneActor : public AActor
{
public:
	GENERATED_BODY()

	ASceneActor() = default;

	void InitDefaultComponents();

	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
    void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
    void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
};

UCLASS()
class APlayerStart : public AActor
{
public:
	GENERATED_BODY()

	APlayerStart() = default;

	void InitDefaultComponents() override;
};

UCLASS()
class AFogActor : public AActor
{
public:
	GENERATED_BODY()

	AFogActor() = default;

	void InitDefaultComponents();

	UHeightFogComponent* GetFogComponent() const { return FogComp; }

private:
	UHeightFogComponent* FogComp = nullptr;
	UBillboardComponent* BillboardComp = nullptr;
};

UCLASS()
class AStaticMeshActor : public AActor
{
public:
	GENERATED_BODY()

	AStaticMeshActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class ASkeletalMeshActor : public AActor
{
public:
	GENERATED_BODY()

	ASkeletalMeshActor() = default;

	void InitDefaultComponents();

	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComp; }

private:
    USkeletalMeshComponent* SkeletalMeshComp = nullptr;
};

UCLASS()
class ASubUVActor : public AActor
{
public:
    GENERATED_BODY()

    ASubUVActor() = default;

    void InitDefaultComponents();
};

UCLASS()
class ATextRenderActor : public AActor
{
public:
    GENERATED_BODY()

    ATextRenderActor() = default;

    void InitDefaultComponents();
};

UCLASS()
class ABillboardActor : public AActor
{
public:
    GENERATED_BODY()

	ABillboardActor() = default;

    void InitDefaultComponents();
};

UCLASS()
class ADecalActor : public AActor
{
public:
	GENERATED_BODY()

	ADecalActor() = default;

	void InitDefaultComponents();
};

UCLASS()
class AFireballActor : public AActor {
public:
    GENERATED_BODY()

	AFireballActor() = default;
	
	void InitDefaultComponents();
};

UCLASS()
class ADecalSpotLightActor : public AActor {
public:
	GENERATED_BODY()

	ADecalSpotLightActor() = default;

	void InitDefaultComponents();

	void Tick(float DeltaTime) override;

	const float GetRange() const { return Range; }
	void SetRange(float InRange) { Range = InRange; }

	const float GetAngle() const { return Angle; }
	void SetAngle(float InAngle) { Angle = InAngle; }

private:
	UDecalComponent* DecalComp = nullptr;

	float Range = 10.0f;
	float Angle = 30.0f;
};

UCLASS()
class ALightActor : public AActor
{
public:
    GENERATED_BODY()

    ALightActor() = default;

    ULightComponent* GetLight() const;
    void SetLight(ULightComponent* InLight);

	UBillboardComponent* GetBillboard() const { return BillboardComp; }
	void SetBillboard(UBillboardComponent* InBillboard) { BillboardComp = InBillboard; }

protected:
    ULightComponent* LightComp = nullptr;
	UBillboardComponent* BillboardComp = nullptr;
};

UCLASS()
class AAmbientLightActor : public ALightActor
{
public:
    GENERATED_BODY()

    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;
};

UCLASS()
class ADirectionalLightActor : public ALightActor
{
public:
    GENERATED_BODY()

    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;
};

UCLASS()
class APointLightActor : public ALightActor
{
public:
    GENERATED_BODY()

    virtual void InitDefaultComponents() override;
    virtual void Tick(float DeltaTime) override;
};

UCLASS()
class ASpotlightActor : public APointLightActor 
{
public:
	GENERATED_BODY()

	void InitDefaultComponents() override;
	void Tick(float DeltaTime) override;
};

UCLASS()
class ABullet : public AActor
{
public:
    GENERATED_BODY()

    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;

	void SetProjectileVelocity(FVector NewVelocity);

private:
    UProjectileMovementComponent* ProjectileComp = nullptr;
};

UCLASS()
class ABladeSlash : public AActor
{
public:
    GENERATED_BODY()

    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;
};

UCLASS()
class ADestructibleActor : public AActor
{
public:
    GENERATED_BODY()

	// 데이터를 입력으로 받아 초기화
	void InitDestructibleActor(UStaticMesh* StaticMesh);
    void InitDestructibleActor(UProceduralMeshComponent* InProcMeshComp);

	// 따로 StaticMesh 지정 안할 시 임의의 StaticMesh 로 초기화
    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;

	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
    void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
    void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	void PostDuplicate(UObject* Original) override;

	uint32 GetSliceCount() const { return SliceCount; }
    void SetSliceCount(uint32 NewSliceCount) { SliceCount = NewSliceCount; }

private:
	UProceduralMeshComponent* ProcMeshComp = nullptr;
    UBoxComponent* BoxComponent = nullptr;
	// 물리 시뮬레이션 흉내용
    UProjectileMovementComponent* ProjMoveComp = nullptr;
	// 현재까지 잘려진 횟수
	uint32 SliceCount = 0;
};

UCLASS()
class ABoundsBoxActor : public AActor
{
public:
    GENERATED_BODY()

    void InitDefaultComponents() override;

	void Tick(float DeltaTime) override;

    void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
    void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
    void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

    void PostDuplicate(UObject* Original) override;
private:
    UBoxComponent* BoxComponent = nullptr;
};

class AMainSceneDestructibleActor;

UCLASS()
class UMainSceneDestructibleComponent : public UActorComponent
{
public:
    GENERATED_BODY()

    void BeginPlay() override;
    void Serialize(FArchive& Ar) override;
protected:
    void TickComponent(float DeltaTime) override;

private:
    float GetRealDeltaTime(float DeltaTime) const;
    bool StartSlice();

    UPROPERTY(EditAnywhere, DisplayName="Auto Start")
    bool bAutoStart = true;
    UPROPERTY(EditAnywhere, DisplayName="Slice Duration", Min=0.05, Max=10.0, Speed=0.05)
    float SliceDuration = 1.0f;
    UPROPERTY(EditAnywhere, DisplayName="Slice Speed", Min=0.0, Max=10.0, Speed=0.05)
    float SliceSpeed = 1.2f;
    UPROPERTY(EditAnywhere, DisplayName="Patrol Amplitude", Min=0.0, Max=10.0, Speed=0.01)
    float PatrolAmplitude = 0.18f;
    UPROPERTY(EditAnywhere, DisplayName="Patrol Speed", Min=0.0, Max=20.0, Speed=0.05)
    float PatrolSpeed = 1.15f;
    UPROPERTY(EditAnywhere, DisplayName="Slice Count", Min=1.0, Max=12.0, Speed=1.0)
    int32 SliceCount = 5;
    UPROPERTY(EditAnywhere, DisplayName="Presentation Trigger", Min=0.0, Max=1.0, Speed=0.01, Animatable)
    float PresentationTrigger = 0.0f;
    bool bPresentationTriggerConsumed = false;

    bool bSlicePending = false;
    bool bSliced = false;
    float Elapsed = 0.0f;
    float PatrolElapsed = 0.0f;
    TArray<AMainSceneDestructibleActor*> Fragments;
    TArray<FVector> FragmentStartLocations;
    TArray<FVector> FragmentTargetLocations;
};

UCLASS()
class AMainSceneDestructibleActor : public AActor
{
public:
    GENERATED_BODY()

    void InitDefaultComponents() override;
    void PostComponentRegistered(UActorComponent* Comp) override;
    void PostDuplicate(UObject* Original) override;

    TArray<AMainSceneDestructibleActor*> SliceForMainScene(const FVector& PlanePointWorld, const FVector& PlaneNormalWorld, float SeparateSpeed);
    void StopPresentationMotion();

private:
    void InitFromStaticMesh(UStaticMesh* StaticMesh, bool bAddPresentationComponent);
    void InitFromProceduralMesh(UProceduralMeshComponent* InProcMeshComp, bool bAddPresentationComponent);
    void RebindComponents();

    UProceduralMeshComponent* ProcMeshComp = nullptr;
    UBoxComponent* BoxComponent = nullptr;
    UProjectileMovementComponent* ProjMoveComp = nullptr;
    UMainSceneDestructibleComponent* PresentationComponent = nullptr;
};
