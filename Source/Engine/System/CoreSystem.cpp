#include "CoreSystem.h"
#include "Core/TimeSystem.h"
#include "Core/LogSystem.h"
#include "Core/MemorySystem.h"
#include "Core/TaskSystem.h"
#include "Core/TestSystem.h"
#include "FileSystem/FileSystem.h"
#include "GameSystem/GameSystem.h"
#include "GameSystem/AssetSystem.h"
#include "PhysicsSystem/PhysicsSystem.h"
#include "GameSystem/InputSystem.h"
#include "RenderingFrontend/RenderingFrontend.h"
#if defined INNO_PLATFORM_WIN
#include "WinWindow/WinWindowSystem.h"
#include "RenderingBackend/DX11RenderingBackend/DX11RenderingBackend.h"
#include "RenderingBackend/DX12RenderingBackend/DX12RenderingBackend.h"
#endif
#if !defined INNO_PLATFORM_MAC && defined INNO_RENDERER_OPENGL
#include "RenderingBackend/GLRenderingBackend/GLRenderingBackend.h"
#endif
#if defined INNO_PLATFORM_MAC
#include "MacWindow/MacWindowSystem.h"
#include "RenderingBackend/MTRenderingBackend/MTRenderingBackend.h"
#endif
#if defined INNO_PLATFORM_LINUX
#include "LinuxWindow/LinuxWindowSystem.h"
#endif
#if defined INNO_RENDERER_VULKAN
#include "RenderingBackend/VKRenderingBackend/VKRenderingBackend.h"
#endif

#include "ImGuiWrapper/ImGuiWrapper.h"

INNO_SYSTEM_EXPORT ICoreSystem* g_pCoreSystem;

#define createSubSystemInstanceDefi( className ) \
m_##className = std::make_unique<Inno##className>(); \
if (!m_##className.get()) \
{ \
	return false; \
} \

#define subSystemSetup( className ) \
if (!g_pCoreSystem->get##className()->setup()) \
{ \
	return false; \
} \
g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, std::string(#className) + " setup finished."); \

#define subSystemInit( className ) \
if (!g_pCoreSystem->get##className()->initialize()) \
{ \
	return false; \
} \

#define subSystemUpdate( className ) \
if (!g_pCoreSystem->get##className()->update()) \
{ \
m_objectStatus = ObjectStatus::Suspended; \
return false; \
}

#define subSystemTerm( className ) \
if (!g_pCoreSystem->get##className()->terminate()) \
{ \
	return false; \
} \

#define subSystemGetDefi( className ) \
I##className * InnoCoreSystem::get##className() \
{ \
	return InnoCoreSystemNS::m_##className.get(); \
} \

INNO_PRIVATE_SCOPE InnoCoreSystemNS
{
	InitConfig parseInitConfig(const std::string& arg);
	bool createSubSystemInstance(void* appHook, void* extraHook, char* pScmdline);
	bool setup(void* appHook, void* extraHook, char* pScmdline);
	bool initialize();
	bool update();
	bool terminate();

	InitConfig m_initConfig;

	std::unique_ptr<ITimeSystem> m_TimeSystem;
	std::unique_ptr<ILogSystem> m_LogSystem;
	std::unique_ptr<IMemorySystem> m_MemorySystem;
	std::unique_ptr<ITaskSystem> m_TaskSystem;
	std::unique_ptr<ITestSystem> m_TestSystem;
	std::unique_ptr<IFileSystem> m_FileSystem;
	std::unique_ptr<IGameSystem> m_GameSystem;
	std::unique_ptr<IAssetSystem> m_AssetSystem;
	std::unique_ptr<IPhysicsSystem> m_PhysicsSystem;
	std::unique_ptr<IInputSystem> m_InputSystem;
	std::unique_ptr<IWindowSystem> m_WindowSystem;
	std::unique_ptr<IRenderingFrontend> m_RenderingFrontend;
	std::unique_ptr<IRenderingBackend> m_RenderingBackend;

	ObjectStatus m_objectStatus = ObjectStatus::Terminated;

	bool m_showImGui = false;
	std::function<void()> f_toggleshowImGui;

	std::atomic<bool> m_isRendering = false;
	std::atomic<bool> m_allowRender = false;

	float m_tickTime = 0;
}

InitConfig InnoCoreSystemNS::parseInitConfig(const std::string& arg)
{
	InitConfig l_result;

	if (arg == "")
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "CoreSystem: No arguments found, use default settings.");
		return l_result;
	}

	auto l_engineModeArgPos = arg.find("mode");

	if (l_engineModeArgPos == std::string::npos)
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "CoreSystem: No engine mode argument found, use default game mode.");
	}
	else
	{
		std::string l_engineModeArguments = arg.substr(l_engineModeArgPos + 5);
		l_engineModeArguments = l_engineModeArguments.substr(0, 1);

		if (l_engineModeArguments == "0")
		{
			l_result.engineMode = EngineMode::GAME;
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_VERBOSE, "CoreSystem: Launch in game mode.");
		}
		else if (l_engineModeArguments == "1")
		{
			l_result.engineMode = EngineMode::EDITOR;
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_VERBOSE, "CoreSystem: Launch in editor mode.");
		}
		else
		{
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "CoreSystem: Unsupported engine mode.");
		}
	}

	auto l_renderingBackendArgPos = arg.find("renderer");

	if (l_renderingBackendArgPos == std::string::npos)
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "CoreSystem: No rendering backend argument found, use default OpenGL rendering backend.");
	}
	else
	{
		std::string l_rendererArguments = arg.substr(l_renderingBackendArgPos + 9);
		l_rendererArguments = l_rendererArguments.substr(0, 1);

		if (l_rendererArguments == "0")
		{
#if !defined INNO_PLATFORM_MAC && defined INNO_RENDERER_OPENGL
			l_result.renderingBackend = RenderingBackend::GL;
#else
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "CoreSystem: OpenGL is not supported on current platform, no rendering backend will be launched.");
#endif
		}
		else if (l_rendererArguments == "1")
		{
#if defined INNO_PLATFORM_WIN
			l_result.renderingBackend = RenderingBackend::DX11;
#else
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "CoreSystem: DirectX 11 is not supported on current platform, use default OpenGL rendering backend.");
#endif
		}
		else if (l_rendererArguments == "2")
		{
#if defined INNO_PLATFORM_WIN
			l_result.renderingBackend = RenderingBackend::DX12;
#else
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "CoreSystem: DirectX 12 is not supported on current platform, use default OpenGL rendering backend.");
#endif
		}
		else if (l_rendererArguments == "3")
		{
#if defined INNO_RENDERER_VULKAN
			l_result.renderingBackend = RenderingBackend::VK;
#else
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "CoreSystem: Vulkan is not supported on current platform, use default OpenGL rendering backend.");
#endif
		}
		else if (l_rendererArguments == "4")
		{
#if defined INNO_PLATFORM_MAC
			l_result.renderingBackend = RenderingBackend::MT;
#else
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "CoreSystem: Metal is not supported on current platform, use default OpenGL rendering backend.");
#endif
		}
		else
		{
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "CoreSystem: Unsupported rendering backend, use default OpenGL rendering backend.");
		}
	}

	return l_result;
}

bool InnoCoreSystemNS::createSubSystemInstance(void* appHook, void* extraHook, char* pScmdline)
{
	createSubSystemInstanceDefi(TimeSystem);
	createSubSystemInstanceDefi(LogSystem);
	createSubSystemInstanceDefi(MemorySystem);
	createSubSystemInstanceDefi(TaskSystem);

	createSubSystemInstanceDefi(TestSystem);
	createSubSystemInstanceDefi(FileSystem);
	createSubSystemInstanceDefi(GameSystem);
	createSubSystemInstanceDefi(AssetSystem);
	createSubSystemInstanceDefi(PhysicsSystem);
	createSubSystemInstanceDefi(InputSystem);

	std::string l_windowArguments = pScmdline;
	m_initConfig = parseInitConfig(l_windowArguments);

#if defined INNO_PLATFORM_WIN
	m_WindowSystem = std::make_unique<WinWindowSystem>();
	if (!m_WindowSystem.get())
	{
		return false;
	}
#endif
#if defined INNO_PLATFORM_MAC
	m_WindowSystem = std::make_unique<MacWindowSystem>();
	if (!m_WindowSystem.get())
	{
		return false;
	}
#endif
#if defined INNO_PLATFORM_LINUX
	m_WindowSystem = std::make_unique<LinuxWindowSystem>();
	if (!m_WindowSystem.get())
	{
		return false;
	}
#endif

	m_RenderingFrontend = std::make_unique<InnoRenderingFrontend>();
	if (!m_RenderingFrontend.get())
	{
		return false;
	}

	switch (m_initConfig.renderingBackend)
	{
	case RenderingBackend::GL:
#if !defined INNO_PLATFORM_MAC && defined INNO_RENDERER_OPENGL
		m_RenderingBackend = std::make_unique<GLRenderingBackend>();
		if (!m_RenderingBackend.get())
		{
			return false;
		}
#endif
		break;
	case RenderingBackend::DX11:
#if defined INNO_PLATFORM_WIN
		m_RenderingBackend = std::make_unique<DX11RenderingBackend>();
		if (!m_RenderingBackend.get())
		{
			return false;
		}
#endif
		break;
	case RenderingBackend::DX12:
#if defined INNO_PLATFORM_WIN
		m_RenderingBackend = std::make_unique<DX12RenderingBackend>();
		if (!m_RenderingBackend.get())
		{
			return false;
		}
#endif
		break;
	case RenderingBackend::VK:
#if defined INNO_RENDERER_VULKAN
		m_RenderingBackend = std::make_unique<VKRenderingBackend>();
		if (!m_RenderingBackend.get())
		{
			return false;
		}
#endif
		break;
	case RenderingBackend::MT:
#if defined INNO_PLATFORM_MAC
		m_RenderingBackend = std::make_unique<MTRenderingBackend>();
		if (!m_RenderingBackend.get())
		{
			return false;
		}
#endif
		break;
	default:
		break;
	}

	// Objective-C++ bridge class instances passed as the 1st and 2nd parameters of setup()
#if defined INNO_PLATFORM_MAC
	auto l_windowSystem = reinterpret_cast<MacWindowSystem*>(m_WindowSystem.get());
	auto l_windowSystemBridge = reinterpret_cast<MacWindowSystemBridge*>(appHook);

	l_windowSystem->setBridge(l_windowSystemBridge);

	auto l_renderingBackendSystem = reinterpret_cast<MTRenderingBackend*>(m_RenderingBackend.get());
	auto l_renderingBackendSystemBridge = reinterpret_cast<MTRenderingBackendBridge*>(extraHook);

	l_renderingBackendSystem->setBridge(l_renderingBackendSystemBridge);
#endif

	return true;
}

bool InnoCoreSystemNS::setup(void* appHook, void* extraHook, char* pScmdline)
{
	if (!InnoCoreSystemNS::createSubSystemInstance(appHook, extraHook, pScmdline))
	{
		return false;
	}

	subSystemSetup(TimeSystem);
	subSystemSetup(LogSystem);
	subSystemSetup(MemorySystem);
	subSystemSetup(TaskSystem);

	subSystemSetup(TestSystem);

	f_toggleshowImGui = [&]() {
		m_showImGui = !m_showImGui;
	};
	g_pCoreSystem->getInputSystem()->addButtonStatusCallback(ButtonData{ INNO_KEY_I, ButtonStatus::PRESSED }, &f_toggleshowImGui);

	if (!m_WindowSystem->setup(appHook, extraHook))
	{
		return false;
	}
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "WindowSystem setup finished.");

	if (!m_RenderingBackend->setup())
	{
		return false;
	}
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "RenderingBackend setup finished.");

	if (!m_RenderingFrontend->setup(m_RenderingBackend.get()))
	{
		return false;
	}
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "RenderingFrontend setup finished.");

	if (!ImGuiWrapper::get().setup())
	{
		return false;
	}

	subSystemSetup(AssetSystem);
	subSystemSetup(FileSystem);
	subSystemSetup(GameSystem);
	subSystemSetup(PhysicsSystem);
	subSystemSetup(InputSystem);

	m_objectStatus = ObjectStatus::Created;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "Engine setup finished.");
	return true;
}

bool InnoCoreSystemNS::initialize()
{
	subSystemInit(TimeSystem);
	subSystemInit(LogSystem);
	subSystemInit(MemorySystem);
	subSystemInit(TaskSystem);

	subSystemInit(TestSystem);

	subSystemInit(FileSystem);
	subSystemInit(GameSystem);
	subSystemInit(AssetSystem);
	subSystemInit(PhysicsSystem);
	subSystemInit(InputSystem);

	m_WindowSystem->initialize();
	m_RenderingBackend->initialize();
	m_RenderingFrontend->initialize();
	ImGuiWrapper::get().initialize();

	m_objectStatus = ObjectStatus::Activated;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "Engine has been initialized.");
	return true;
}

bool InnoCoreSystemNS::update()
{
	auto l_tickStartTime = m_TimeSystem->getCurrentTimeFromEpoch();
	subSystemUpdate(TimeSystem);
	subSystemUpdate(LogSystem);
	subSystemUpdate(MemorySystem);
	subSystemUpdate(TaskSystem);

	subSystemUpdate(TestSystem);

	subSystemUpdate(FileSystem);
	subSystemUpdate(GameSystem);
	subSystemUpdate(AssetSystem);
	subSystemUpdate(PhysicsSystem);
	subSystemUpdate(InputSystem);

	if (m_WindowSystem->getStatus() == ObjectStatus::Activated)
	{
		if (g_pCoreSystem->getFileSystem()->isLoadingScene())
		{
			return true;
		}

		m_WindowSystem->update();

		if (!m_allowRender)
		{
			m_RenderingFrontend->update();

			m_allowRender = true;
		}

		if (!m_isRendering && m_allowRender)
		{
			m_allowRender = false;

			m_isRendering = true;

			m_RenderingBackend->update();

			m_RenderingBackend->render();

			if (m_showImGui)
			{
				ImGuiWrapper::get().update();
			}

			m_WindowSystem->swapBuffer();

			m_isRendering = false;
		}

		g_pCoreSystem->getGameSystem()->saveComponentsCapture();

		auto l_tickEndTime = m_TimeSystem->getCurrentTimeFromEpoch();

		m_tickTime = float(l_tickEndTime - l_tickStartTime) / 1000.0f;

		return true;
	}
	else
	{
		m_objectStatus = ObjectStatus::Suspended;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "Engine is stand-by.");
		return false;
	}
}

bool InnoCoreSystemNS::terminate()
{
	if (!ImGuiWrapper::get().terminate())
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "GuiSystem can't be terminated!");
		return false;
	}

	if (!m_RenderingBackend->terminate())
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackend can't be terminated!");
		return false;
	}
	if (!m_RenderingFrontend->terminate())
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingFrontend can't be terminated!");
		return false;
	}

	if (!m_WindowSystem->terminate())
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "WindowSystem can't be terminated!");
		return false;
	}

	subSystemTerm(InputSystem);
	subSystemTerm(PhysicsSystem);
	subSystemTerm(AssetSystem);
	subSystemTerm(GameSystem);
	subSystemTerm(FileSystem);

	subSystemTerm(TestSystem);

	subSystemTerm(TaskSystem);
	subSystemTerm(MemorySystem);
	subSystemTerm(LogSystem);
	subSystemTerm(TimeSystem);

	m_objectStatus = ObjectStatus::Terminated;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "Engine has been terminated.");

	return true;
}

bool InnoCoreSystem::setup(void* appHook, void* extraHook, char* pScmdline)
{
	g_pCoreSystem = this;

	return InnoCoreSystemNS::setup(appHook, extraHook, pScmdline);
}

bool InnoCoreSystem::initialize()
{
	return InnoCoreSystemNS::initialize();
}

bool InnoCoreSystem::update()
{
	return InnoCoreSystemNS::update();
}

bool InnoCoreSystem::terminate()
{
	return InnoCoreSystemNS::terminate();
}

ObjectStatus InnoCoreSystem::getStatus()
{
	return InnoCoreSystemNS::m_objectStatus;
}

subSystemGetDefi(TimeSystem);
subSystemGetDefi(LogSystem);
subSystemGetDefi(MemorySystem);
subSystemGetDefi(TaskSystem);

subSystemGetDefi(TestSystem);

subSystemGetDefi(FileSystem);
subSystemGetDefi(GameSystem);
subSystemGetDefi(AssetSystem);
subSystemGetDefi(PhysicsSystem);
subSystemGetDefi(InputSystem);

IWindowSystem * InnoCoreSystem::getWindowSystem()
{
	return InnoCoreSystemNS::m_WindowSystem.get();
}

IRenderingFrontend * InnoCoreSystem::getRenderingFrontend()
{
	return InnoCoreSystemNS::m_RenderingFrontend.get();
}

IRenderingBackend * InnoCoreSystem::getRenderingBackend()
{
	return InnoCoreSystemNS::m_RenderingBackend.get();
}

InitConfig InnoCoreSystem::getInitConfig()
{
	return InnoCoreSystemNS::m_initConfig;
}

float InnoCoreSystem::getTickTime()
{
	return  InnoCoreSystemNS::m_tickTime;
}
