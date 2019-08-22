#include "MemorySystem.h"
#include "InnoMemory.h"
#include "InnoLogger.h"

namespace InnoMemorySystemNS
{
	ObjectStatus m_objectStatus = ObjectStatus::Terminated;
}

bool InnoMemorySystem::setup()
{
	InnoMemorySystemNS::m_objectStatus = ObjectStatus::Created;
	return true;
}

bool InnoMemorySystem::initialize()
{
	if (InnoMemorySystemNS::m_objectStatus == ObjectStatus::Created)
	{
		InnoMemorySystemNS::m_objectStatus = ObjectStatus::Activated;
		InnoLogger::Log(LogLevel::Success, "MemorySystem has been initialized.");
		return true;
	}
	else
	{
		InnoLogger::Log(LogLevel::Error, "MemorySystem: Object is not created!");
		return false;
	}
}

bool InnoMemorySystem::update()
{
	if (InnoMemorySystemNS::m_objectStatus == ObjectStatus::Activated)
	{
		return true;
	}
	else
	{
		InnoMemorySystemNS::m_objectStatus = ObjectStatus::Suspended;
		return false;
	}
}

bool InnoMemorySystem::terminate()
{
	InnoMemorySystemNS::m_objectStatus = ObjectStatus::Terminated;
	InnoLogger::Log(LogLevel::Success, "MemorySystem has been terminated.");
	return true;
}
ObjectStatus InnoMemorySystem::getStatus()
{
	return InnoMemorySystemNS::m_objectStatus;
}

void * InnoMemorySystem::allocate(size_t size)
{
	return InnoMemory::Allocate(size);
}

bool InnoMemorySystem::deallocate(void * ptr)
{
	InnoMemory::Deallocate(ptr);
	return true;
}

void * InnoMemorySystem::reallocate(void * ptr, size_t size)
{
	return InnoMemory::Reallocate(ptr, size);
}