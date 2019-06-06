#include "ImGuiWrapperWinVK.h"
#include "../../component/WinWindowSystemComponent.h"

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE ImGuiWrapperWinVKNS
{
	ObjectStatus m_objectStatus = ObjectStatus::Terminated;
}

bool ImGuiWrapperWinVK::setup()
{
	ImGuiWrapperWinVKNS::m_objectStatus = ObjectStatus::Activated;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "ImGuiWrapperWinVK setup finished.");

	return true;
}

bool ImGuiWrapperWinVK::initialize()
{
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "ImGuiWrapperWinVK has been initialized.");

	return true;
}

bool ImGuiWrapperWinVK::newFrame()
{
	return true;
}

bool ImGuiWrapperWinVK::render()
{
	return true;
}

bool ImGuiWrapperWinVK::terminate()
{
	ImGuiWrapperWinVKNS::m_objectStatus = ObjectStatus::Terminated;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "ImGuiWrapperWinVK has been terminated.");

	return true;
}

ObjectStatus ImGuiWrapperWinVK::getStatus()
{
	return ImGuiWrapperWinVKNS::m_objectStatus;
}

void ImGuiWrapperWinVK::showRenderResult(RenderPassType renderPassType)
{
}

ImTextureID ImGuiWrapperWinVK::getFileExplorerIconTextureID(const FileExplorerIconType iconType)
{
	return nullptr;
}