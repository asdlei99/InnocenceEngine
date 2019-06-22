#pragma once
#include "IGameSystem.h"

#define spawnComponentImplDecl( className ) \
className* spawn##className(const InnoEntity* parentEntity, ObjectSource objectSource, ObjectUsage objectUsage) override;

#define registerComponentImplDecl( className ) \
void registerComponent(className* rhs, const InnoEntity* parentEntity) override;

#define destroyComponentImplDecl( className ) \
bool destroy(className* rhs) override;

#define unregisterComponentImplDecl( className ) \
void unregisterComponent(className* rhs) override;

#define getComponentImplDecl( className ) \
className* get##className(const InnoEntity* parentEntity) override;

#define getComponentContainerImplDecl( className ) \
std::vector<className*>& get##className##s() override;

INNO_CONCRETE InnoGameSystem : INNO_IMPLEMENT IGameSystem
{
public:
	INNO_CLASS_CONCRETE_NON_COPYABLE(InnoGameSystem);

	bool setup() override;
	bool initialize() override;
	bool update() override;
	bool terminate() override;

	ObjectStatus getStatus() override;

	spawnComponentImplDecl(TransformComponent);
	spawnComponentImplDecl(VisibleComponent);
	spawnComponentImplDecl(DirectionalLightComponent);
	spawnComponentImplDecl(PointLightComponent);
	spawnComponentImplDecl(SphereLightComponent);
	spawnComponentImplDecl(CameraComponent);

	registerComponentImplDecl(TransformComponent);
	registerComponentImplDecl(VisibleComponent);
	registerComponentImplDecl(DirectionalLightComponent);
	registerComponentImplDecl(PointLightComponent);
	registerComponentImplDecl(SphereLightComponent);
	registerComponentImplDecl(CameraComponent);

	destroyComponentImplDecl(TransformComponent);
	destroyComponentImplDecl(VisibleComponent);
	destroyComponentImplDecl(DirectionalLightComponent);
	destroyComponentImplDecl(PointLightComponent);
	destroyComponentImplDecl(SphereLightComponent);
	destroyComponentImplDecl(CameraComponent);

	unregisterComponentImplDecl(TransformComponent);
	unregisterComponentImplDecl(VisibleComponent);
	unregisterComponentImplDecl(DirectionalLightComponent);
	unregisterComponentImplDecl(PointLightComponent);
	unregisterComponentImplDecl(SphereLightComponent);
	unregisterComponentImplDecl(CameraComponent);

	getComponentImplDecl(TransformComponent);
	getComponentImplDecl(VisibleComponent);
	getComponentImplDecl(DirectionalLightComponent);
	getComponentImplDecl(PointLightComponent);
	getComponentImplDecl(SphereLightComponent);
	getComponentImplDecl(CameraComponent);

	getComponentContainerImplDecl(TransformComponent);
	getComponentContainerImplDecl(VisibleComponent);
	getComponentContainerImplDecl(DirectionalLightComponent);
	getComponentContainerImplDecl(PointLightComponent);
	getComponentContainerImplDecl(SphereLightComponent);
	getComponentContainerImplDecl(CameraComponent);

	std::string getGameName() override;
	TransformComponent* getRootTransformComponent() override;
	const std::vector<InnoEntity*>& getEntities() override;
	const EntityChildrenComponentsMetadataMap& getEntityChildrenComponentsMetadataMap() override;

	void saveComponentsCapture() override;

	InnoEntity* createEntity(const EntityName& entityName, ObjectSource objectSource, ObjectUsage objectUsage) override;
	bool removeEntity(const InnoEntity* entity) override;
	InnoEntity* getEntity(const EntityName& entityName) override;
};
