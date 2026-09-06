#include "UParticle.h"
#include <iostream>


UParticle::UParticle()
{
	SetActive(false);
	HalfW = 0.015f;
	HalfH = 0.015f;
}

UParticle::~UParticle()
{
}

void UParticle::Spawn(FVector Pos, FVector Vel, FColor Col, float Life)
{
	
	this->Position = Pos;   
	this->Velocity = Vel;   
	this->Color = Col;      
	this->LifeTime = Life;
	this->MaxLife = Life;
	this->SetActive(true);
}

void UParticle::Update(float deltaTime)
{
	if (!IsActive()) 
		return;
	Position =Position + (Velocity * deltaTime);

	LifeTime -= deltaTime*3.0f;
	if (LifeTime <= 0)
	{
		if (LifeTime <= 0) SetActive(false);
	}
}

void UParticle::Render(URenderer& renderer)
{

	if (!IsActive()) return;

	float scaleRatio = LifeTime / MaxLife;

	renderer.UpdateConstant(
		Position,
		FVector(HalfW * scaleRatio, HalfH * scaleRatio, 1.0f),
		Color,
		- 3.0f
	);
	renderer.RenderRectangle();
}

