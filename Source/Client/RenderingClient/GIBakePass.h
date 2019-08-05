#pragma once
#include "../../Engine/RenderingServer/IRenderingServer.h"

// Sample point on geometry surface
struct Surfel
{
	vec4 pos;
	vec4 normal;
	vec4 albedo;
	vec4 MRAT;

	bool operator==(const Surfel &other) const
	{
		return (pos == other.pos);
	}
};

// 1x1x1 m^3 of surfels
using SurfelGrid = Surfel;

// 4x4x4 m^3 of surfels
struct Brick
{
	AABB boundBox;
	unsigned int surfelRangeBegin;
	unsigned int surfelRangeEnd;

	bool operator==(const Brick &other) const
	{
		return (boundBox.m_center == other.boundBox.m_center);
	}
};

struct BrickFactor
{
	float basisWeight;
	unsigned int brickIndex;
};

struct Probe
{
	vec4 pos;
	SH9 skyVisibility;
	SH9 radiance;
	unsigned int brickFactorRangeBegin;
	unsigned int brickFactorRangeEnd;
};

namespace GIBakePass
{
	bool Setup();
	bool Initialize();
	bool Bake();
	bool PrepareCommandList();
	bool ExecuteCommandList();

	RenderPassDataComponent* GetRPDC();
	ShaderProgramComponent* GetSPC();
};
