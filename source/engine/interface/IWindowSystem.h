#pragma once
#include "common/stdafx.h"
#include "ISystem.h"
#include "component/InnoMath.h"
#include "common/config.h"

class IWindowSystem : public ISystem
{
public:
	virtual ~IWindowSystem() {};
	virtual vec4 calcMousePositionInWorldSpace() = 0;
	virtual void swapBuffer() = 0;
#if defined(INNO_RENDERER_DX)
	virtual void setup(void* appInstance, char* commandLineArg, int showMethod) = 0;
#endif
};