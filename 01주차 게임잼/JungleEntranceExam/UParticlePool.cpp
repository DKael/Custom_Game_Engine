#include "UParticlePool.h"


UParticlePool::UParticlePool(int size) : poolSize(size)
{
	for (int i = 0; i < size; ++i) {
		mPool.push_back(new UParticle());
	}
}

UParticlePool::~UParticlePool()
{
	for (UParticle*& particle : mPool)
		delete particle;
	mPool.clear();
}

UParticle* UParticlePool::GetInactiveParticle()
{ 
	for (UParticle*& particlce : mPool)
	{
		if (!particlce->IsActive())
		{
			return particlce;
		}
	}
	return nullptr;
}

void UParticlePool::Update(float deltaTime)
{
	for (UParticle*& particle : mPool)
	{
		if (particle->IsActive())
		{
			particle->Update(deltaTime);
		}
	}
}

void UParticlePool::Render(URenderer& renderer)
{
	for (UParticle*& particle : mPool)
	{
		if (particle->IsActive())
		{
			particle->Render(renderer);
		}
	}
}
