#pragma once

#include <functional>
#include <utility>
#include "Object/Object.h"
#include "Core/Singleton.h"

struct FObjectFactoryEntry
{
	std::function<UObject*()> Spawner;
	std::function<const UClass*()> ClassGetter;
};

// Different from UFactory class
class FObjectFactory : public TSingleton<FObjectFactory>
{
	friend class TSingleton<FObjectFactory>;

public:
	void Register(
		const char* TypeName,
		std::function<UObject*()> Spawner,
		std::function<const UClass*()> ClassGetter = nullptr)
	{
		Registry[TypeName] = FObjectFactoryEntry{ std::move(Spawner), std::move(ClassGetter) };
	}

	UObject* Create(const std::string& TypeName) {
		auto Spawner = Registry.find(TypeName);	// Do NOT use array accessor [] here. it will insert a new key if not found.
		return (Spawner != Registry.end() && Spawner->second.Spawner) ? Spawner->second.Spawner() : nullptr;
	}

	void GetRegisteredClasses(TArray<const UClass*>& OutClasses) const {
		for (const auto& [TypeName, Entry] : Registry)
		{
			if (Entry.ClassGetter)
			{
				OutClasses.push_back(Entry.ClassGetter());
			}
		}
	}

	void Shutdown() {
		Registry.clear();
		Registry.rehash(0);
	}

private:
	TMap<std::string, FObjectFactoryEntry> Registry;
};
