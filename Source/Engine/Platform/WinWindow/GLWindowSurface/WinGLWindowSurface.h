#pragma once
#include "../../../Core/IWindowSurface.h"

class WinGLWindowSurface : INNO_IMPLEMENT IWindowSurface
{
public:
	INNO_CLASS_CONCRETE_NON_COPYABLE(WinGLWindowSurface);

	bool setup(void* hInstance, void* hwnd, void* WindowProc) override;
	bool initialize() override;
	bool update() override;
	bool terminate() override;

	ObjectStatus getStatus() override;

	bool swapBuffer() override;
};