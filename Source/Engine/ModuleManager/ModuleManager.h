#pragma once
#include "IModuleManager.h"

INNO_CONCRETE InnoModuleManager : INNO_IMPLEMENT IModuleManager
{
public:
	INNO_CLASS_CONCRETE_NON_COPYABLE(InnoModuleManager);

	bool setup(void* appHook, void* extraHook, char* pScmdline, IGameInstance* gameInstance) override;
	bool initialize() override;
	bool run() override;
	bool terminate() override;

	ObjectStatus getStatus() override;

	ITimeSystem* getTimeSystem() override;
	ILogSystem* getLogSystem() override;
	IMemorySystem* getMemorySystem() override;
	ITaskSystem* getTaskSystem() override;
	ITestSystem* getTestSystem() override;
	IFileSystem* getFileSystem() override;
	IEntityManager* getEntityManager() override;
	IComponentManager* getComponentManager(ComponentType componentType) override;
	ISceneHierarchyManager* getSceneHierarchyManager() override;
	IAssetSystem* getAssetSystem() override;
	IPhysicsSystem* getPhysicsSystem() override;
	IEventSystem* getEventSystem() override;
	IWindowSystem* getWindowSystem() override;
	IRenderingFrontend* getRenderingFrontend() override;
	IRenderingBackend* getRenderingBackend() override;

	InitConfig getInitConfig() override;
	float getTickTime() override;
	const FixedSizeString<128>& getApplicationName() override;
};