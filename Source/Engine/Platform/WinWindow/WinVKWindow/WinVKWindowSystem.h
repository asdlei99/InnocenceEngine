#pragma once
#include "../IWinWindowSystem.h"

class WinVKWindowSystem : INNO_IMPLEMENT IWinWindowSystem
{
public:
	INNO_CLASS_CONCRETE_NON_COPYABLE(WinVKWindowSystem);

	bool setup(void* hInstance, void* hwnd, void* WindowProc) override;
	bool initialize() override;
	bool update() override;
	bool terminate() override;

	ObjectStatus getStatus() override;

	void swapBuffer() override;
};