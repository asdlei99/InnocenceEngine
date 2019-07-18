#pragma once
#include "../../Common/InnoType.h"
#include "../../Component/DX12RenderPassComponent.h"

INNO_PRIVATE_SCOPE DX12FinalBlendPass
{
	bool setup();
	bool initialize();
	bool update();
	bool render();
	bool terminate();
	bool resize();
	bool reloadShaders();

	DX12RenderPassComponent* getDX12RPC();
}
