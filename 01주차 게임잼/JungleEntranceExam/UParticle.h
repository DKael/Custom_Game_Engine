#pragma once
#include "UDiagram.h"

class UParticle : public UDiagram
{
public:
	UParticle();
	~UParticle();


	void Spawn(FVector Pos, FVector Vel, FColor Col, float Life);
	void Update(float deltaTime) override;
	void Render(URenderer& renderer) override;
	bool IsActive() const { return bActive; }
	void SetActive(bool InActive) { bActive = InActive; }


	virtual void ApplyWallCollision() {}
	virtual void ApplyGravity(float deltaTime, const FVector& gravity) {}
	virtual bool CheckCollision(const UDiagram * Other) { return false; }
public:
	bool bActive = true;
	FVector Velocity;
	FVector Position;
	float HalfW, HalfH;
	float LifeTime;
	float MaxLife;
	FColor Color;
};