#include "InnoApplication.h"
#include "../System/CoreSystem.h"
#include "../../Game/GameInstance.h"

namespace InnoApplication
{
	std::unique_ptr<InnoCoreSystem> m_pCoreSystem;
	std::unique_ptr<GameInstance> m_pGameInstance;
}

bool InnoApplication::Setup(void* appHook, void* extraHook, char* pScmdline)
{
	m_pCoreSystem = std::make_unique<InnoCoreSystem>();
	if (!m_pCoreSystem.get())
	{
		return false;
	}

	m_pGameInstance = std::make_unique<GameInstance>();
	if (!m_pGameInstance.get())
	{
		return false;
	}

	if (!m_pCoreSystem.get()->setup(appHook, extraHook, pScmdline, m_pGameInstance.get()))
	{
		return false;
	}

	return true;
}

bool InnoApplication::Initialize()
{
	if (!m_pCoreSystem->initialize())
	{
		return false;
	}

	return true;
}

bool InnoApplication::Run()
{
	if (!m_pCoreSystem->run())
	{
		return false;
	}
	return true;
}

bool InnoApplication::Terminate()
{
	if (!m_pCoreSystem->terminate())
	{
		return false;
	}
	return true;
}