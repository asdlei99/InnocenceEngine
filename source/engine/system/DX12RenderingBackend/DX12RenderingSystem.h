#pragma once
#include "../IRenderingBackendSystem.h"
#include "../../exports/InnoSystem_Export.h"

class DX12RenderingSystem : INNO_IMPLEMENT IRenderingBackendSystem
{
public:
	INNO_CLASS_CONCRETE_NON_COPYABLE(DX12RenderingSystem);

	bool setup(IRenderingFrontendSystem* renderingFrontend) override;
	bool initialize() override;
	bool update() override;
	bool terminate() override;

	ObjectStatus getStatus() override;

	bool resize() override;
	bool reloadShader(RenderPassType renderPassType) override;
	bool bakeGI() override;
};
