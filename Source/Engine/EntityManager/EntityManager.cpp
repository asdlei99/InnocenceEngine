#include "EntityManager.h"
#include "../Core/InnoLogger.h"

#include "../Interface/IModuleManager.h"

extern IModuleManager* g_pModuleManager;

namespace EntityManagerNS
{
	const size_t m_MaxEntity = 65536;
	IObjectPool* m_EntityPool;
	ThreadSafeVector<InnoEntity*> m_Entities;

	std::function<void()> f_SceneLoadingStartCallback;
}

using namespace EntityManagerNS;

bool InnoEntityManager::Setup()
{
	m_EntityPool = InnoMemory::CreateObjectPool<InnoEntity>(m_MaxEntity);

	f_SceneLoadingStartCallback = [&]() {
		for (auto i : m_Entities)
		{
			if (i->m_ObjectOwnership == ObjectOwnership::Client)
			{
				i->m_ObjectStatus = ObjectStatus::Terminated;
				m_EntityPool->Destroy(i);
			}
		}

		m_Entities.erase(
			std::remove_if(m_Entities.begin(), m_Entities.end(),
				[&](auto val) {
			return val->m_ObjectOwnership == ObjectOwnership::Client;
		}), m_Entities.end());
	};

	g_pModuleManager->getFileSystem()->addSceneLoadingStartCallback(&f_SceneLoadingStartCallback);

	return true;
}

bool InnoEntityManager::Initialize()
{
	return true;
}

bool InnoEntityManager::Simulate()
{
	return true;
}

bool InnoEntityManager::Terminate()
{
	return true;
}

InnoEntity * InnoEntityManager::Spawn(ObjectSource objectSource, ObjectOwnership objectUsage, const char* entityName)
{
	auto l_Entity = InnoMemory::Spawn<InnoEntity>(m_EntityPool);
	if (l_Entity)
	{
		auto l_EntityID = InnoMath::createEntityID();

		l_Entity->m_ObjectStatus = ObjectStatus::Created;
		m_Entities.emplace_back(l_Entity);

		l_Entity->m_EntityID = l_EntityID;
		l_Entity->m_EntityName = entityName;
		l_Entity->m_ObjectSource = objectSource;
		l_Entity->m_ObjectOwnership = objectUsage;
		l_Entity->m_ObjectStatus = ObjectStatus::Activated;
	}

	InnoLogger::Log(LogLevel::Verbose, "EntityManager: Entity ", l_Entity->m_EntityName.c_str(), " has been created.");
	return l_Entity;
}

bool InnoEntityManager::Destroy(InnoEntity * entity)
{
	m_Entities.eraseByValue(entity);
	InnoLogger::Log(LogLevel::Verbose, "EntityManager: Entity ", entity->m_EntityName.c_str(), " has been removed.");
	m_EntityPool->Destroy(entity);
	return true;
}

std::optional<InnoEntity*> InnoEntityManager::Find(const char * entityName)
{
	auto l_FindResult = std::find_if(
		m_Entities.begin(),
		m_Entities.end(),
		[&](auto val) -> bool {
		return val->m_EntityName == entityName;
	});

	if (l_FindResult != m_Entities.end())
	{
		return *l_FindResult;
	}
	else
	{
		InnoLogger::Log(LogLevel::Warning, "EntityManager: Can't find entity by name ", entityName, "!");
		return std::nullopt;
	}
}

const std::vector<InnoEntity*>&  InnoEntityManager::GetEntities()
{
	return m_Entities.getRawData();
}

uint64_t InnoEntityManager::AcquireUUID()
{
	static uint64_t l_UUID = 0;
	l_UUID++;
	return l_UUID;
}