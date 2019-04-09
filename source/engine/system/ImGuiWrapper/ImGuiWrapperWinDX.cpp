#include "ImGuiWrapperWinDX.h"
#include "../../component/WinWindowSystemComponent.h"
#include "../../component/DX11RenderingSystemComponent.h"
#include "../DX11RenderingBackend/DX11OpaquePass.h"

#include "../../third-party/ImGui/imgui_impl_win32.h"
#include "../../third-party/ImGui/imgui_impl_dx11.h"

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE ImGuiWrapperWinDXNS
{
	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;
}

bool ImGuiWrapperWinDX11::setup()
{
	ImGuiWrapperWinDXNS::m_objectStatus = ObjectStatus::ALIVE;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "ImGuiWrapperWinDX setup finished.");

	return true;
}

bool ImGuiWrapperWinDX11::initialize()
{
	ImGui_ImplWin32_Init(WinWindowSystemComponent::get().m_hwnd);
	ImGui_ImplDX11_Init(DX11RenderingSystemComponent::get().m_device, DX11RenderingSystemComponent::get().m_deviceContext);
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "ImGuiWrapperWinDX has been initialized.");

	return true;
}

bool ImGuiWrapperWinDX11::newFrame()
{
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX11_NewFrame();
	return true;
}

bool ImGuiWrapperWinDX11::render()
{
	DX11RenderingSystemComponent::get().m_deviceContext->OMSetRenderTargets(
		1,
		&DX11RenderingSystemComponent::get().m_renderTargetView,
		NULL);

	DX11RenderingSystemComponent::get().m_deviceContext->RSSetViewports(
		1,
		&DX11RenderingSystemComponent::get().m_viewport);

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return true;
}

bool ImGuiWrapperWinDX11::terminate()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGuiWrapperWinDXNS::m_objectStatus = ObjectStatus::SHUTDOWN;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "ImGuiWrapperWinDX has been terminated.");

	return true;
}

ObjectStatus ImGuiWrapperWinDX11::getStatus()
{
	return ImGuiWrapperWinDXNS::m_objectStatus;
}

void ImGuiWrapperWinDX11::showRenderResult()
{
	auto l_screenResolution = g_pCoreSystem->getVisionSystem()->getRenderingFrontend()->getScreenResolution();
	auto l_renderTargetSize = ImVec2((float)l_screenResolution.x / 4.0f, (float)l_screenResolution.y / 4.0f);

	ImGui::Begin("Opaque Pass", 0, ImGuiWindowFlags_AlwaysAutoResize);
	{
		{
			ImGui::BeginChild("World Space Position(RGB) + Metallic(A)", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
			ImGui::Text("World Space Position(RGB) + Metallic(A)");
			ImGui::Image(ImTextureID(DX11OpaquePass::getDX11RPC()->m_DXTDCs[0]->m_SRV), l_renderTargetSize, ImVec2(0.0, 0.0), ImVec2(1.0, 1.0));
			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("World Space Normal(RGB) + Roughness(A)", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
			ImGui::Text("World Space Normal(RGB) + Roughness(A)");
			ImGui::Image(ImTextureID(DX11OpaquePass::getDX11RPC()->m_DXTDCs[1]->m_SRV), l_renderTargetSize, ImVec2(0.0, 0.0), ImVec2(1.0, 1.0));
			ImGui::EndChild();
		}
		{
			ImGui::BeginChild("Albedo(RGB) + Ambient Occlusion(A)", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
			ImGui::Text("Albedo(RGB) + Ambient Occlusion(A)");
			ImGui::Image(ImTextureID(DX11OpaquePass::getDX11RPC()->m_DXTDCs[2]->m_SRV), l_renderTargetSize, ImVec2(0.0, 0.0), ImVec2(1.0, 1.0));
			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("Screen Space Motion Vector(RGB) + Transparency(A)", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
			ImGui::Text("Screen Space Motion Vector(RGB) + Transparency(A)");
			ImGui::Image(ImTextureID(DX11OpaquePass::getDX11RPC()->m_DXTDCs[3]->m_SRV), l_renderTargetSize, ImVec2(0.0, 0.0), ImVec2(1.0, 1.0));
			ImGui::EndChild();
		}
	}
	ImGui::End();
}

ImTextureID ImGuiWrapperWinDX11::getFileExplorerIconTextureID(const FileExplorerIconType iconType)
{
	switch (iconType)
	{
	case FileExplorerIconType::OBJ:
		return ImTextureID(DX11RenderingSystemComponent::get().m_iconTemplate_OBJ->m_SRV); break;
	case FileExplorerIconType::PNG:
		return ImTextureID(DX11RenderingSystemComponent::get().m_iconTemplate_PNG->m_SRV); break;
	case FileExplorerIconType::SHADER:
		return ImTextureID(DX11RenderingSystemComponent::get().m_iconTemplate_SHADER->m_SRV); break;
	case FileExplorerIconType::UNKNOWN:
		return ImTextureID(DX11RenderingSystemComponent::get().m_iconTemplate_UNKNOWN->m_SRV); break;
	default:
		return nullptr; break;
	}
}