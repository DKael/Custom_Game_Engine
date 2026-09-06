#pragma once
#include <vector>

#include "UParticle.h"
class UParticlePool {
private:
	std::vector <UParticle*> mPool;
	int poolSize;

public:
	UParticlePool(int size);
	~UParticlePool();

	UParticle* GetInactiveParticle();

	void Update(float deltaTime);
	void Render(URenderer& renderer);

};