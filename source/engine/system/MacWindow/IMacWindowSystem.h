#pragma once
#include "../../common/InnoType.h"
#include "../../common/InnoClassTemplate.h"

INNO_INTERFACE IMacWindowSystem
{
public:
	INNO_CLASS_INTERFACE_NON_COPYABLE(IMacWindowSystem);

	virtual bool setup() = 0;
	virtual bool initialize() = 0;
	virtual bool update() = 0;
	virtual bool terminate() = 0;

	virtual ObjectStatus getStatus() = 0;

	virtual void swapBuffer() = 0;
};
