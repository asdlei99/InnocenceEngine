#include "TaskSystem.h"
#include "MemorySystem.h"
#include "LogSystem.h"

void InnoTaskSystem::setup()
{
	m_threadPool = InnoMemorySystem::spawn<InnoThreadPool>();
	m_TaskSystemStatus = objectStatus::ALIVE;
}

void InnoTaskSystem::initialize()
{
	InnoLogSystem::printLog("TaskSystem has been initialized.");
}

void InnoTaskSystem::update()
{
}

void InnoTaskSystem::shutdown()
{
	m_TaskSystemStatus = objectStatus::STANDBY;
	delete m_threadPool;
	m_TaskSystemStatus = objectStatus::SHUTDOWN;
	InnoLogSystem::printLog("TaskSystem has been shutdown.");
}
