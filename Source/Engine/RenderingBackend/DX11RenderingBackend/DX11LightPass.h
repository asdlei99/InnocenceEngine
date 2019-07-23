#pragma once
#include "../../Common/InnoType.h"
#include "../../Component/DX11RenderPassComponent.h"

namespace DX11LightPass
{
	bool initialize();

	bool update();

	bool resize();

	bool reloadShaders();

	DX11RenderPassComponent* getDX11RPC();
}