#pragma once
#include "../../component/AssetSystemSingletonComponent.h"

namespace MeshDataSystem
{
	void setup();
	void initialize();
	void update();
	void terminate();

	void addUnitCube(MeshDataComponent& meshDataComponent);
	void addUnitSphere(MeshDataComponent& meshDataComponent);
	void addUnitQuad(MeshDataComponent& meshDataComponent);
	void addUnitLine(MeshDataComponent& meshDataComponent);

	vec4 findMaxVertex(const MeshDataComponent& meshDataComponent);
	vec4 findMinVertex(const MeshDataComponent& meshDataComponent);
};
