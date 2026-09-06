#include "UBullet.h"

UBullet::UBullet(int _BulletDirection)
	: BulletDirection(_BulletDirection)
{
	Scale *= BulletDirection;
}

UBullet::~UBullet()
{

}

void UBullet::Update(float DeltaTime)
{
	Location.y += (Speed * BulletDirection * DeltaTime);
}

void UBullet::Render(URenderer& renderer)
{
	renderer.UpdateConstant(Location, FVector(Scale, Scale, 0));
}

void UBullet::ApplyWallCollision()
{

}

void UBullet::ApplyGravity(float deltaTime, const FVector& gravity)
{

}

bool UBullet::CheckCollision(const UDiagram* Other)
{
	return false;
}

void UBullet::SetLocation(const FVector& _Location)
{
	Location = _Location;
}

void UBullet::SetLocation(FVector&& _Location)
{
	Location = std::move(_Location);
}

FVector UBullet::GetLocation() const
{
	return Location;
}

FVector& UBullet::GetLocation()
{
	return Location;
}

void UBullet::SetSpeed(const float _Speed)
{
	Speed = _Speed;
}

float UBullet::GetSpeed() const
{
	return Speed;
}

void UBullet::SetScale(const float _Scale)
{
	Scale = _Scale;
}

float UBullet::GetScale() const
{
	return Scale;
}

void UBullet::SetIsHit(const bool _IsHit)
{
	IsHit = _IsHit;
}

bool UBullet::GetIsHit() const
{
	return IsHit;
}

void UBullet::SetIsFlying(const bool _IsFlying)
{
	IsFlying = _IsFlying;
}

bool UBullet::GetIsFlying() const
{
	return IsFlying;
}

int UBullet::GetBulletDirection() const
{
	return BulletDirection;
}

bool UBullet::CheckBlockHit(const UBlock& Block, float DeltaTime) const
{
	FVector PastLocation(Location.x, Location.y - (Speed * BulletDirection * DeltaTime), Location.z);

	if (BulletDirection > 0)
	{
		if (PastLocation.y + UBullet::Height <= Block.CenterY && Block.CenterY <= Location.y + UBullet::Height)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (Location.y - UBullet::Height <= Block.CenterY && Block.CenterY <= PastLocation.y - UBullet::Height)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}