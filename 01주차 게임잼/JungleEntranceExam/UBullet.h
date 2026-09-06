#pragma once

#include "UDiagram.h"
#include "UBlock.h"

class UBullet :public UDiagram
{
private:
	FVector Location;
	float Speed{ 10.0f };
	float Scale{ 0.3f };
	int	BulletDirection{ 0 };
	bool IsHit{ false };
	bool IsFlying{ false };

public:
	inline static const float Height{ 0.08f };
	inline static const float Width{ 0.02f };

	UBullet(int _BulletDirection);

	virtual ~UBullet() override;

	virtual void Update(float DeltaTime);

	virtual void Render(URenderer& renderer);

	virtual void ApplyWallCollision();

	virtual void ApplyGravity(float DeltaTime, const FVector& gravity);

	virtual bool CheckCollision(const UDiagram* Other) override;

	void SetLocation(const FVector& _Location);

	void SetLocation(FVector&& _Location);

	FVector GetLocation() const;

	FVector& GetLocation();

	void SetSpeed(const float _Speed);

	float GetSpeed() const;

	void SetScale(const float _Scale);

	float GetScale() const;

	void SetIsHit(const bool _IsHit);

	bool GetIsHit() const;

	void SetIsFlying(const bool _IsFlying);

	bool GetIsFlying() const;

	int GetBulletDirection() const;

	bool CheckBlockHit(const UBlock& Block, float DeltaTime) const;
};

