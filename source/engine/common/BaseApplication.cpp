#include "BaseApplication.h"
#include "../system/MemorySystem.h"
#include "../system/LogSystem.h"
#include "../system/TaskSystem.h"
#include "../system/TimeSystem.h"
#include "../system/GameSystem.h"
#include "../system/AssetSystem.h"
#include "../system/PhysicsSystem.h"
#include "../system/VisionSystem.h"

namespace InnoApplication
{
	objectStatus m_objectStatus = objectStatus::SHUTDOWN;
}

void InnoApplication::setup()
{
	InnoMemorySystem::setup();
	InnoLogSystem::setup();
	InnoTaskSystem::setup();
	InnoTimeSystem::setup();
	InnoLogSystem::printLog("MemorySystem setup finished.");
	InnoLogSystem::printLog("LogSystem setup finished.");
	InnoLogSystem::printLog("TaskSystem setup finished.");
	InnoLogSystem::printLog("TimeSystem setup finished.");
	InnoGameSystem::setup();
	InnoLogSystem::printLog("GameSystem setup finished.");
	InnoAssetSystem::setup();
	InnoLogSystem::printLog("AssetSystem setup finished.");
	InnoPhysicsSystem::setup();
	InnoLogSystem::printLog("PhysicsSystem setup finished.");
	InnoVisionSystem::setup();
	InnoLogSystem::printLog("VisionSystem setup finished.");

	m_objectStatus = objectStatus::ALIVE;

	InnoLogSystem::printLog("Engine setup finished.");
}



void InnoApplication::initialize()
{
	InnoMemorySystem::initialize();
	InnoLogSystem::initialize();
	InnoTaskSystem::initialize();
	InnoTimeSystem::initialize();
	InnoGameSystem::initialize();
	InnoAssetSystem::initialize();
	InnoPhysicsSystem::initialize();
	InnoVisionSystem::initialize();

	InnoLogSystem::printLog("Engine has been initialized.");
}

void InnoApplication::update()
{
	// time System should update without any limitation.
	InnoTimeSystem::update();

	InnoTaskSystem::update();

	InnoGameSystem::update();

	if (InnoVisionSystem::getStatus() == objectStatus::ALIVE)
	{
		if (InnoGameSystem::needRender())
		{
			InnoPhysicsSystem::update();
			InnoVisionSystem::update();
		}
	}
	else
	{
		m_objectStatus = objectStatus::STANDBY;

		InnoLogSystem::printLog("Engine is stand-by.");
	}
}

void InnoApplication::shutdown()
{
	InnoVisionSystem::shutdown();
	InnoGameSystem::shutdown();
	InnoPhysicsSystem::shutdown();
	InnoAssetSystem::shutdown();
	InnoTimeSystem::shutdown();
	InnoLogSystem::printLog("Engine has been shutdown.");
	InnoLogSystem::shutdown();
	InnoMemorySystem::shutdown();
	m_objectStatus = objectStatus::SHUTDOWN;
	std::this_thread::sleep_for(std::chrono::seconds(2));
}

objectStatus InnoApplication::getStatus()
{
	return m_objectStatus;
}

