#include "ImGuiWrapperVK.h"

#include "../ModuleManager/IModuleManager.h"
extern IModuleManager* g_pModuleManager;

namespace ImGuiWrapperVKNS
{
	ObjectStatus m_ObjectStatus = ObjectStatus::Terminated;
}

bool ImGuiWrapperVK::setup()
{
	ImGuiWrapperVKNS::m_ObjectStatus = ObjectStatus::Activated;
	g_pModuleManager->getLogSystem()->Log(LogLevel::Success, "ImGuiWrapperVK setup finished.");

	return true;
}

bool ImGuiWrapperVK::initialize()
{
	g_pModuleManager->getLogSystem()->Log(LogLevel::Success, "ImGuiWrapperVK has been initialized.");

	return true;
}

bool ImGuiWrapperVK::newFrame()
{
	return true;
}

bool ImGuiWrapperVK::render()
{
	return true;
}

bool ImGuiWrapperVK::terminate()
{
	ImGuiWrapperVKNS::m_ObjectStatus = ObjectStatus::Terminated;
	g_pModuleManager->getLogSystem()->Log(LogLevel::Success, "ImGuiWrapperVK has been terminated.");

	return true;
}

ObjectStatus ImGuiWrapperVK::getStatus()
{
	return ImGuiWrapperVKNS::m_ObjectStatus;
}

void ImGuiWrapperVK::showRenderResult(RenderPassType renderPassType)
{
}

ImTextureID ImGuiWrapperVK::getFileExplorerIconTextureID(const FileExplorerIconType iconType)
{
	return nullptr;
}