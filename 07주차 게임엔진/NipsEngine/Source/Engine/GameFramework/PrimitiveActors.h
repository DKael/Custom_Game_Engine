#pragma once

#include "AActor.h"

class UTextRenderComponent;

class ACubeActor : public AActor
{
public:
	DECLARE_CLASS(ACubeActor, AActor)
	ACubeActor() = default;

	void InitDefaultComponents();
};

class ASphereActor : public AActor
{
public:
	DECLARE_CLASS(ASphereActor, AActor)
	ASphereActor() = default;

	void InitDefaultComponents();
};

class APlaneActor : public AActor
{
public:
	DECLARE_CLASS(APlaneActor, AActor)
	APlaneActor() = default;

	void InitDefaultComponents();
};

class AAttachTestActor : public AActor
{
public:
	DECLARE_CLASS(AAttachTestActor, AActor)
	AAttachTestActor() = default;

	void InitDefaultComponents();
};

class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	AStaticMeshActor() = default;

	void InitDefaultComponents();
};

class ASubUVActor : public AActor
{
public:
    DECLARE_CLASS(ASubUVActor, AActor)
    ASubUVActor() = default;

    void InitDefaultComponents();
};

class ATextRenderActor : public AActor
{
public:
    DECLARE_CLASS(ATextRenderActor, AActor)
    ATextRenderActor() = default;

    void InitDefaultComponents();
};

class ABillboardActor : public AActor
{
public:
    DECLARE_CLASS(ABillboardActor, AActor)
	ABillboardActor() = default;

    void InitDefaultComponents();
};

class ADecalActor : public AActor
{
public:
    DECLARE_CLASS(ADecalActor, AActor)
    ADecalActor() = default;

    void InitDefaultComponents();
};

class UHeightFogComponent;

class AExponentialHeightFog : public AActor
{
public:
    DECLARE_CLASS(AExponentialHeightFog, AActor)
    AExponentialHeightFog() = default;

    void InitDefaultComponents();

    UHeightFogComponent* GetFogComponent() const { return FogComponent; }

private:
    UHeightFogComponent* FogComponent = nullptr;
};

class AFakeSpotlightActor : public AActor
{
public:
    DECLARE_CLASS(AFakeSpotlightActor, AActor)
    AFakeSpotlightActor() = default;

    void InitDefaultComponents();
};