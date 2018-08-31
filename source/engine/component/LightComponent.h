#pragma once
#include "BaseComponent.h"

enum class lightType {DIRECTIONAL, POINT, SPOT};

class LightComponent : public BaseComponent
{
public:
	LightComponent() :
		m_lightType(lightType::POINT),
		m_direction(vec4<double>(0.0, 0.0, 0.0, 0.0)),
		m_constantFactor(1.0),
		m_linearFactor(0.14),
		m_quadraticFactor(0.07),
		m_radius(1.0),
		m_color(vec4<double>(1.0, 1.0, 1.0, 1.0)),
		m_drawAABB(false) {};
	~LightComponent() {};

	lightType m_lightType;
	vec4<double> m_direction;

	double m_constantFactor;
	double m_linearFactor;
	double m_quadraticFactor;
	double m_radius;

	vec4<double> m_color;

	bool m_drawAABB;

	std::vector<AABB> m_AABBs;
	std::vector<meshID> m_AABBMeshIDs;
};

