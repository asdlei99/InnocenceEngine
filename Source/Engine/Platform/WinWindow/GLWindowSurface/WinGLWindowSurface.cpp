#include "WinGLWindowSurface.h"
#include "../../../Component/WinWindowSystemComponent.h"

#include "glad/glad.h"
#include "GL/glext.h"
#include "GL/wglext.h"

#include "../../../ModuleManager/IModuleManager.h"

extern IModuleManager* g_pModuleManager;

namespace WinGLWindowSurfaceNS
{
	bool setup(void* hInstance, void* hwnd, void* WindowProc);
	bool initialize();
	bool update();
	bool terminate();

	HGLRC m_HGLRC;

	ObjectStatus m_objectStatus = ObjectStatus::Terminated;
	InitConfig m_initConfig;
}

bool WinGLWindowSurfaceNS::setup(void* hInstance, void* hwnd, void* WindowProc)
{
	m_initConfig = g_pModuleManager->getInitConfig();

	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(wcex));
	wcex.cbSize = sizeof(wcex);
	wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wcex.lpfnWndProc = (WNDPROC)WindowProc;
	wcex.hInstance = WinWindowSystemComponent::get().m_hInstance;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.lpszClassName = WinWindowSystemComponent::get().m_applicationName;

	auto l_windowClass = MAKEINTATOM(RegisterClassEx(&wcex));

	// create temporary window
	HWND fakeWND = CreateWindow(
		l_windowClass,
		"Fake Window",
		WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		0, 0,						// position x, y
		1, 1,						// width, height
		NULL, NULL,					// parent window, menu
		WinWindowSystemComponent::get().m_hInstance, NULL);			// instance, param

	HDC fakeDC = GetDC(fakeWND);	// Device Context

	PIXELFORMATDESCRIPTOR fakePFD;
	ZeroMemory(&fakePFD, sizeof(fakePFD));
	fakePFD.nSize = sizeof(fakePFD);
	fakePFD.nVersion = 1;
	fakePFD.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	fakePFD.iPixelType = PFD_TYPE_RGBA;
	fakePFD.cColorBits = 32;
	fakePFD.cAlphaBits = 8;
	fakePFD.cDepthBits = 24;

	const int fakePFDID = ChoosePixelFormat(fakeDC, &fakePFD);

	if (fakePFDID == 0)
	{
		m_objectStatus = ObjectStatus::Created;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: ChoosePixelFormat() failed.");
		return false;
	}

	if (SetPixelFormat(fakeDC, fakePFDID, &fakePFD) == false)
	{
		m_objectStatus = ObjectStatus::Created;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: SetPixelFormat() failed.");
		return false;
	}

	HGLRC fakeRC = wglCreateContext(fakeDC);

	// Rendering Context
	if (fakeRC == 0)
	{
		m_objectStatus = ObjectStatus::Created;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: wglCreateContext() failed.");
		return false;
	}

	if (wglMakeCurrent(fakeDC, fakeRC) == false)
	{
		m_objectStatus = ObjectStatus::Created;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: wglMakeCurrent() failed.");
		return false;
	}

	// load function pointer for context creation
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
	wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));
	if (wglChoosePixelFormatARB == nullptr)
	{
		m_objectStatus = ObjectStatus::Created;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: wglGetProcAddress(wglChoosePixelFormatARB) failed.");
		return false;
	}

	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
	wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));
	if (wglCreateContextAttribsARB == nullptr)
	{
		m_objectStatus = ObjectStatus::Created;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: wglGetProcAddress(wglCreateContextAttribsARB) failed.");
		return false;
	}

	// Determine the resolution of the clients desktop screen.
	auto l_screenResolution = g_pModuleManager->getRenderingFrontend()->getScreenResolution();
	auto l_screenWidth = (int)l_screenResolution.x;
	auto l_screenHeight = (int)l_screenResolution.y;

	auto l_posX = (GetSystemMetrics(SM_CXSCREEN) - l_screenWidth) / 2;
	auto l_posY = (GetSystemMetrics(SM_CYSCREEN) - l_screenHeight) / 2;

	if (m_initConfig.engineMode == EngineMode::GAME)
	{
		auto l_windowName = g_pModuleManager->getApplicationName();

		// create a new window and context
		WinWindowSystemComponent::get().m_hwnd = CreateWindow(
			l_windowClass, (LPCSTR)l_windowName.c_str(),	// class name, window name
			WS_OVERLAPPEDWINDOW,	// styles
			l_posX, l_posY,		// posx, posy. If x is set to CW_USEDEFAULT y is ignored
			l_screenWidth, l_screenHeight,	// width, height
			NULL, NULL,						// parent window, menu
			WinWindowSystemComponent::get().m_hInstance, NULL);				// instance, param
	}

	WinWindowSystemComponent::get().m_HDC = GetDC(WinWindowSystemComponent::get().m_hwnd);

	const int pixelAttribs[] = {
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
		WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
		WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
		WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
		WGL_COLOR_BITS_ARB, 32,
		WGL_ALPHA_BITS_ARB, 8,
		WGL_DEPTH_BITS_ARB, 24,
		WGL_STENCIL_BITS_ARB, 8,
		WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
		WGL_SAMPLES_ARB, 4,
		0
	};

	int pixelFormatID;
	UINT numFormats;
	const bool status = wglChoosePixelFormatARB(WinWindowSystemComponent::get().m_HDC, pixelAttribs, NULL, 1, &pixelFormatID, &numFormats);

	if (status == false || numFormats == 0)
	{
		m_objectStatus = ObjectStatus::Created;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: wglChoosePixelFormatARB() failed.");
		return false;
	}

	PIXELFORMATDESCRIPTOR PFD;
	DescribePixelFormat(WinWindowSystemComponent::get().m_HDC, pixelFormatID, sizeof(PFD), &PFD);
	SetPixelFormat(WinWindowSystemComponent::get().m_HDC, pixelFormatID, &PFD);

	const int major_min = 4, minor_min = 6;
	std::vector<int> contextAttribs =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, major_min,
		WGL_CONTEXT_MINOR_VERSION_ARB, minor_min,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		WGL_CONTEXT_FLAGS_ARB
	};

#ifdef _DEBUG
	contextAttribs.emplace_back(WGL_CONTEXT_DEBUG_BIT_ARB);
#endif // _DEBUG
	contextAttribs.emplace_back(0);

	m_HGLRC = wglCreateContextAttribsARB(WinWindowSystemComponent::get().m_HDC, 0, &contextAttribs[0]);

	if (m_HGLRC == NULL)
	{
		m_objectStatus = ObjectStatus::Created;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: wglCreateContextAttribsARB() failed.");
		return false;
	}

	// delete temporary context and window
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(fakeRC);
	ReleaseDC(fakeWND, fakeDC);
	DestroyWindow(fakeWND);

	if (!wglMakeCurrent(WinWindowSystemComponent::get().m_HDC, m_HGLRC))
	{
		m_objectStatus = ObjectStatus::Created;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: wglMakeCurrent() failed.");
		return false;
	}

	// init opengl loader here (extra safe version)
	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGL())
	{
		m_objectStatus = ObjectStatus::Created;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: Failed to initialize GLAD.");
		return false;
	}

	auto l_renderingConfig = g_pModuleManager->getRenderingFrontend()->getRenderingConfig();

	if (!l_renderingConfig.VSync)
	{
		PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;
		wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));
		if (wglSwapIntervalEXT == nullptr)
		{
			m_objectStatus = ObjectStatus::Created;
			g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "WinWindowSystem: wglGetProcAddress(wglSwapIntervalEXT) failed.");
			return false;
		}
		wglSwapIntervalEXT(0);
	}

	if (m_initConfig.engineMode == EngineMode::GAME)
	{
		ShowWindow(WinWindowSystemComponent::get().m_hwnd, true);
		SetForegroundWindow(WinWindowSystemComponent::get().m_hwnd);
		SetFocus(WinWindowSystemComponent::get().m_hwnd);
	}

	m_objectStatus = ObjectStatus::Activated;
	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "WinGLWindowSurface setup finished.");

	return true;
}

bool WinGLWindowSurfaceNS::initialize()
{
	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "WinGLWindowSurface has been initialized.");
	return true;
}

bool WinGLWindowSurfaceNS::update()
{
	return true;
}

bool WinGLWindowSurfaceNS::terminate()
{
	m_objectStatus = ObjectStatus::Terminated;
	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "WinGLWindowSurfaceNS has been terminated.");

	return true;
}

bool WinGLWindowSurface::setup(void* hInstance, void* hwnd, void* WindowProc)
{
	return WinGLWindowSurfaceNS::setup(hInstance, hwnd, WindowProc);
}

bool WinGLWindowSurface::initialize()
{
	return WinGLWindowSurfaceNS::initialize();
}

bool WinGLWindowSurface::update()
{
	return WinGLWindowSurfaceNS::update();
}

bool WinGLWindowSurface::terminate()
{
	return WinGLWindowSurfaceNS::terminate();
}

ObjectStatus WinGLWindowSurface::getStatus()
{
	return WinGLWindowSurfaceNS::m_objectStatus;
}

bool WinGLWindowSurface::swapBuffer()
{
	SwapBuffers(WinWindowSystemComponent::get().m_HDC);

	return true;
}