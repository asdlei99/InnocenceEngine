#pragma once
#include "IImGuiRenderer.h"

class ImGuiRendererGL : public IImGuiRenderer
{
public:
	INNO_CLASS_CONCRETE_NON_COPYABLE(ImGuiRendererGL);

	bool setup() override;
	bool initialize() override;
	bool newFrame() override;
	bool render() override;
	bool terminate() override;

	ObjectStatus getStatus() override;

	void showRenderResult(RenderPassType renderPassType) override;
};