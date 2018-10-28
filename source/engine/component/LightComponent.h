#pragma once
#include "../common/InnoType.h"
#include "../common/InnoMath.h"

enum class lightType {DIRECTIONAL, POINT, SPOT};

class LightComponent
{
public:
	LightComponent() :
		m_lightType(lightType::POINT),
		m_direction(vec4(0.0, 0.0, 0.0, 0.0)),
		m_constantFactor(1.0),
		m_linearFactor(0.14),
		m_quadraticFactor(0.07),
		m_radius(1.0),
		m_color(vec4(1.0, 1.0, 1.0, 1.0)),
		m_drawAABB(false) {};
	~LightComponent() {};

	objectStatus m_objectStatus = objectStatus::SHUTDOWN;
	EntityID m_parentEntity;

	lightType m_lightType;
	vec4 m_direction;

	double m_constantFactor;
	double m_linearFactor;
	double m_quadraticFactor;
	double m_radius;

	vec4 m_color;

	bool m_drawAABB;

	std::vector<AABB> m_AABBs;
	std::vector<EntityID> m_AABBMeshIDs;

	std::vector<float> m_shadowSplitPoints;
	std::vector<mat4> m_projectionMatrices;
};

