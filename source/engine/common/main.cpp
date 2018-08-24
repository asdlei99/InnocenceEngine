#include "stdafx.h"
#include "interface/IApplication.h"
#include "config.h"
#include "component/WindowSystemSingletonComponent.h"

extern IApplication* g_pApp;

#if defined(INNO_RENDERER_DX)
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pScmdline, int nCmdshow)
{
	WindowSystemSingletonComponent::getInstance().m_hInstance = hInstance;
	WindowSystemSingletonComponent::getInstance().m_pScmdline = pScmdline;
	WindowSystemSingletonComponent::getInstance().m_nCmdshow = nCmdshow;
#else
int main()
{
#endif
	g_pApp->setup();
	g_pApp->initialize();

	while (g_pApp->getStatus() == objectStatus::ALIVE)
	{
		g_pApp->update();
	}

	g_pApp->shutdown();

	return 0;
}