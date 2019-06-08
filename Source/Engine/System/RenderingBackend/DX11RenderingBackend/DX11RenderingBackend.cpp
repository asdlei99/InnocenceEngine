#include "DX11RenderingBackend.h"

#include "DX11RenderingBackendUtilities.h"

#include "DX11OpaquePass.h"
#include "DX11LightCullingPass.h"
#include "DX11LightPass.h"
#include "DX11SkyPass.h"
#include "DX11PreTAAPass.h"
#include "DX11TAAPass.h"
#include "DX11PostTAAPass.h"
#include "DX11MotionBlurPass.h"
#include "DX11FinalBlendPass.h"

#include "../../../Component/DX11RenderingBackendComponent.h"
#include "../../../Component/WinWindowSystemComponent.h"
#include "../../../Component/RenderingFrontendComponent.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#include "../../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE DX11RenderingBackendNS
{
	bool createPhysicalDevices();
	bool createSwapChain();
	bool createSwapChainDXRPC();

	ObjectStatus m_objectStatus = ObjectStatus::Terminated;
	EntityID m_entityID;

	static DX11RenderingBackendComponent* g_DXRenderingBackendComponent;

	void* m_MeshDataComponentPool;
	void* m_MaterialDataComponentPool;
	void* m_TextureDataComponentPool;

	ThreadSafeQueue<DX11MeshDataComponent*> m_uninitializedMDC;
	ThreadSafeQueue<DX11TextureDataComponent*> m_uninitializedTDC;

	DX11TextureDataComponent* m_iconTemplate_OBJ;
	DX11TextureDataComponent* m_iconTemplate_PNG;
	DX11TextureDataComponent* m_iconTemplate_SHADER;
	DX11TextureDataComponent* m_iconTemplate_UNKNOWN;

	DX11TextureDataComponent* m_iconTemplate_DirectionalLight;
	DX11TextureDataComponent* m_iconTemplate_PointLight;
	DX11TextureDataComponent* m_iconTemplate_SphereLight;

	DX11MeshDataComponent* m_unitLineMDC;
	DX11MeshDataComponent* m_unitQuadMDC;
	DX11MeshDataComponent* m_unitCubeMDC;
	DX11MeshDataComponent* m_unitSphereMDC;
	DX11MeshDataComponent* m_terrainMDC;

	DX11TextureDataComponent* m_basicNormalTDC;
	DX11TextureDataComponent* m_basicAlbedoTDC;
	DX11TextureDataComponent* m_basicMetallicTDC;
	DX11TextureDataComponent* m_basicRoughnessTDC;
	DX11TextureDataComponent* m_basicAOTDC;
}

bool DX11RenderingBackendNS::createPhysicalDevices()
{
	HRESULT result;

	unsigned int numModes;
	unsigned long long stringLength;

	// Create a DirectX graphics interface factory.
	result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&g_DXRenderingBackendComponent->m_factory);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't create DXGI factory!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Use the factory to create an adapter for the primary graphics interface (video card).
	result = g_DXRenderingBackendComponent->m_factory->EnumAdapters(0, &g_DXRenderingBackendComponent->m_adapter);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't create video card adapter!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Enumerate the primary adapter output (monitor).
	result = g_DXRenderingBackendComponent->m_adapter->EnumOutputs(0, &g_DXRenderingBackendComponent->m_adapterOutput);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't create monitor adapter!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	result = g_DXRenderingBackendComponent->m_adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't get DXGI_FORMAT_R8G8B8A8_UNORM fitted monitor!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Create a list to hold all the possible display modes for this monitor/video card combination.
	std::vector<DXGI_MODE_DESC> displayModeList(numModes);

	// Now fill the display mode list structures.
	result = g_DXRenderingBackendComponent->m_adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, &displayModeList[0]);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't fill the display mode list structures!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Now go through all the display modes and find the one that matches the screen width and height.
	// When a match is found store the numerator and denominator of the refresh rate for that monitor.
	auto l_screenResolution = g_pCoreSystem->getRenderingFrontend()->getScreenResolution();

	for (unsigned int i = 0; i < numModes; i++)
	{
		if (displayModeList[i].Width == l_screenResolution.x
			&&
			displayModeList[i].Height == l_screenResolution.y
			)
		{
			g_DXRenderingBackendComponent->m_refreshRate.x = displayModeList[i].RefreshRate.Numerator;
			g_DXRenderingBackendComponent->m_refreshRate.y = displayModeList[i].RefreshRate.Denominator;
		}
	}

	// Get the adapter (video card) description.
	result = g_DXRenderingBackendComponent->m_adapter->GetDesc(&g_DXRenderingBackendComponent->m_adapterDesc);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't get the video card adapter description!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Store the dedicated video card memory in megabytes.
	g_DXRenderingBackendComponent->m_videoCardMemory = (int)(g_DXRenderingBackendComponent->m_adapterDesc.DedicatedVideoMemory / 1024 / 1024);

	// Convert the name of the video card to a character array and store it.
	if (wcstombs_s(&stringLength, g_DXRenderingBackendComponent->m_videoCardDescription, 128, g_DXRenderingBackendComponent->m_adapterDesc.Description, 128) != 0)
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't convert the name of the video card to a character array!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Release the display mode list.
	// displayModeList.clear();

	// Release the adapter output.
	g_DXRenderingBackendComponent->m_adapterOutput->Release();
	g_DXRenderingBackendComponent->m_adapterOutput = 0;

	// Release the adapter.
	g_DXRenderingBackendComponent->m_adapter->Release();
	g_DXRenderingBackendComponent->m_adapter = 0;

	// Release the factory.
	g_DXRenderingBackendComponent->m_factory->Release();
	g_DXRenderingBackendComponent->m_factory = 0;

	return true;
}

bool DX11RenderingBackendNS::createSwapChain()
{
	HRESULT result;
	D3D_FEATURE_LEVEL featureLevel;

	// Initialize the swap chain description.
	ZeroMemory(&g_DXRenderingBackendComponent->m_swapChainDesc, sizeof(g_DXRenderingBackendComponent->m_swapChainDesc));

	// Set to a single back buffer.
	g_DXRenderingBackendComponent->m_swapChainDesc.BufferCount = 1;

	auto l_screenResolution = g_pCoreSystem->getRenderingFrontend()->getScreenResolution();

	// Set the width and height of the back buffer.
	g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.Width = l_screenResolution.x;
	g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.Height = l_screenResolution.y;

	// Set regular 32-bit surface for the back buffer.
	g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Set the refresh rate of the back buffer.
	if (WinWindowSystemComponent::get().m_vsync_enabled)
	{
		g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.RefreshRate.Numerator = g_DXRenderingBackendComponent->m_refreshRate.x;
		g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.RefreshRate.Denominator = g_DXRenderingBackendComponent->m_refreshRate.y;
	}
	else
	{
		g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	}

	// Set the usage of the back buffer.
	g_DXRenderingBackendComponent->m_swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	// Set the handle for the window to render to.
	g_DXRenderingBackendComponent->m_swapChainDesc.OutputWindow = WinWindowSystemComponent::get().m_hwnd;

	// Turn multisampling off.
	g_DXRenderingBackendComponent->m_swapChainDesc.SampleDesc.Count = 1;
	g_DXRenderingBackendComponent->m_swapChainDesc.SampleDesc.Quality = 0;

	// Set to full screen or windowed mode.
	// @TODO: finish this feature
	g_DXRenderingBackendComponent->m_swapChainDesc.Windowed = true;

	// Set the scan line ordering and scaling to unspecified.
	g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// Discard the back buffer contents after presenting.
	g_DXRenderingBackendComponent->m_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Don't set the advanced flags.
	g_DXRenderingBackendComponent->m_swapChainDesc.Flags = 0;

	// Set the feature level to DirectX 11.
	featureLevel = D3D_FEATURE_LEVEL_11_1;

	// Create the swap chain, Direct3D device, and Direct3D device context.
	unsigned int creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG

	ID3D11Device* l_device;
	ID3D11DeviceContext* l_deviceContext;
	IDXGISwapChain* l_swapChain;

	result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, &featureLevel, 1,
		D3D11_SDK_VERSION, &g_DXRenderingBackendComponent->m_swapChainDesc, &l_swapChain, &l_device, NULL, &l_deviceContext);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't create the swap chain/D3D device/D3D device context!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	g_DXRenderingBackendComponent->m_device = reinterpret_cast<ID3D11Device5*>(l_device);
	g_DXRenderingBackendComponent->m_deviceContext = reinterpret_cast<ID3D11DeviceContext4*>(l_deviceContext);
	g_DXRenderingBackendComponent->m_swapChain = reinterpret_cast<IDXGISwapChain4*>(l_swapChain);

	return true;
}

bool DX11RenderingBackendNS::createSwapChainDXRPC()
{
	auto l_imageCount = 1;

	auto l_DXRPC = addDX11RenderPassComponent(m_entityID, "SwapChainDXRPC\\");

	l_DXRPC->m_renderPassDesc = DX11RenderingBackendComponent::get().m_deferredRenderPassDesc;
	l_DXRPC->m_renderPassDesc.RTNumber = l_imageCount;
	l_DXRPC->m_renderPassDesc.useMultipleFramebuffers = false;
	l_DXRPC->m_renderPassDesc.useDepthAttachment = false;
	l_DXRPC->m_renderPassDesc.useStencilAttachment = false;

	// Setup the raster description.
	l_DXRPC->m_rasterizerDesc.AntialiasedLineEnable = true;
	l_DXRPC->m_rasterizerDesc.CullMode = D3D11_CULL_NONE;
	l_DXRPC->m_rasterizerDesc.DepthBias = 0;
	l_DXRPC->m_rasterizerDesc.DepthBiasClamp = 0.0f;
	l_DXRPC->m_rasterizerDesc.DepthClipEnable = true;
	l_DXRPC->m_rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	l_DXRPC->m_rasterizerDesc.FrontCounterClockwise = false;
	l_DXRPC->m_rasterizerDesc.MultisampleEnable = true;
	l_DXRPC->m_rasterizerDesc.ScissorEnable = false;
	l_DXRPC->m_rasterizerDesc.SlopeScaledDepthBias = 0.0f;

	// initialize manually
	bool l_result = true;
	l_result &= reserveRenderTargets(l_DXRPC);

	HRESULT l_hResult;

	// Get the pointer to the back buffer.
	l_hResult = g_DXRenderingBackendComponent->m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&l_DXRPC->m_DXTDCs[0]->m_texture);
	if (FAILED(l_hResult))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't get back buffer pointer!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	l_result &= createRTV(l_DXRPC);

	l_result &= setupPipeline(l_DXRPC);

	l_DXRPC->m_objectStatus = ObjectStatus::Activated;

	DX11RenderingBackendComponent::get().m_swapChainDXRPC = l_DXRPC;

	return true;
}

bool DX11RenderingBackendNS::setup()
{
	m_entityID = InnoMath::createEntityID();

	g_DXRenderingBackendComponent = &DX11RenderingBackendComponent::get();

	auto l_screenResolution = g_pCoreSystem->getRenderingFrontend()->getScreenResolution();

	// general render pass desc
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTNumber = 1;
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.samplerType = TextureSamplerType::SAMPLER_2D;
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.usageType = TextureUsageType::COLOR_ATTACHMENT;
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.pixelDataFormat = TexturePixelDataFormat::RGBA;
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.minFilterMethod = TextureFilterMethod::NEAREST;
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.magFilterMethod = TextureFilterMethod::NEAREST;
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.wrapMethod = TextureWrapMethod::CLAMP_TO_EDGE;
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.width = l_screenResolution.x;
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.height = l_screenResolution.y;
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.pixelDataType = TexturePixelDataType::FLOAT16;

	bool l_result = true;
	l_result = l_result && initializeComponentPool();
	l_result = l_result && createPhysicalDevices();
	l_result = l_result && createSwapChain();

	m_objectStatus = ObjectStatus::Created;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX11RenderingBackend setup finished.");

	return l_result;
}

bool DX11RenderingBackendNS::initialize()
{
	if (DX11RenderingBackendNS::m_objectStatus == ObjectStatus::Created)
	{
		m_MeshDataComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(DX11MeshDataComponent), RenderingFrontendComponent::get().m_maxMeshes);
		m_MaterialDataComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(MaterialDataComponent), RenderingFrontendComponent::get().m_maxMaterials);
		m_TextureDataComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(DX11TextureDataComponent), RenderingFrontendComponent::get().m_maxTextures);

		loadDefaultAssets();

		bool l_result = true;

		l_result = l_result && generateGPUBuffers();
		l_result = l_result && createSwapChainDXRPC();

		DX11OpaquePass::initialize();
		DX11LightCullingPass::initialize();
		DX11LightPass::initialize();
		DX11SkyPass::initialize();
		DX11PreTAAPass::initialize();
		DX11TAAPass::initialize();
		DX11PostTAAPass::initialize();
		DX11MotionBlurPass::initialize();
		DX11FinalBlendPass::initialize();

		DX11RenderingBackendNS::m_objectStatus = ObjectStatus::Activated;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX11RenderingBackend has been initialized.");
		return l_result;
	}
	else
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: Object is not created!");
		return false;
	}
}

void DX11RenderingBackendNS::loadDefaultAssets()
{
	auto l_basicNormalTDC = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//basic_normal.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_basicAlbedoTDC = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//basic_albedo.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::ALBEDO);
	auto l_basicMetallicTDC = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//basic_metallic.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::METALLIC);
	auto l_basicRoughnessTDC = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//basic_roughness.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::ROUGHNESS);
	auto l_basicAOTDC = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//basic_ao.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::AMBIENT_OCCLUSION);

	auto l_iconTemplate_OBJ = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//InnoFileTypeIcons_OBJ.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_PNG = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//InnoFileTypeIcons_PNG.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_SHADER = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//InnoFileTypeIcons_SHADER.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_UNKNOWN = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//InnoFileTypeIcons_UNKNOWN.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);

	auto l_iconTemplate_DirectionalLight = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//InnoWorldEditorIcons_DirectionalLight.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_PointLight = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//InnoWorldEditorIcons_PointLight.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_SphereLight = g_pCoreSystem->getAssetSystem()->loadTexture("Res//Textures//InnoWorldEditorIcons_SphereLight.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);

	m_basicNormalTDC = reinterpret_cast<DX11TextureDataComponent*>(l_basicNormalTDC);
	m_basicAlbedoTDC = reinterpret_cast<DX11TextureDataComponent*>(l_basicAlbedoTDC);
	m_basicMetallicTDC = reinterpret_cast<DX11TextureDataComponent*>(l_basicMetallicTDC);
	m_basicRoughnessTDC = reinterpret_cast<DX11TextureDataComponent*>(l_basicRoughnessTDC);
	m_basicAOTDC = reinterpret_cast<DX11TextureDataComponent*>(l_basicAOTDC);

	m_iconTemplate_OBJ = reinterpret_cast<DX11TextureDataComponent*>(l_iconTemplate_OBJ);
	m_iconTemplate_PNG = reinterpret_cast<DX11TextureDataComponent*>(l_iconTemplate_PNG);
	m_iconTemplate_SHADER = reinterpret_cast<DX11TextureDataComponent*>(l_iconTemplate_SHADER);
	m_iconTemplate_UNKNOWN = reinterpret_cast<DX11TextureDataComponent*>(l_iconTemplate_UNKNOWN);

	m_iconTemplate_DirectionalLight = reinterpret_cast<DX11TextureDataComponent*>(l_iconTemplate_DirectionalLight);
	m_iconTemplate_PointLight = reinterpret_cast<DX11TextureDataComponent*>(l_iconTemplate_PointLight);
	m_iconTemplate_SphereLight = reinterpret_cast<DX11TextureDataComponent*>(l_iconTemplate_SphereLight);

	m_unitLineMDC = addDX11MeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitLine(*m_unitLineMDC);
	m_unitLineMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE_STRIP;
	m_unitLineMDC->m_meshShapeType = MeshShapeType::LINE;
	m_unitLineMDC->m_objectStatus = ObjectStatus::Created;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitLineMDC);

	m_unitQuadMDC = addDX11MeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitQuad(*m_unitQuadMDC);
	// Flip y texture coordinate
	for (auto& i : m_unitQuadMDC->m_vertices)
	{
		i.m_texCoord.y = 1.0f - i.m_texCoord.y;
	}
	m_unitQuadMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_unitQuadMDC->m_meshShapeType = MeshShapeType::QUAD;
	m_unitQuadMDC->m_objectStatus = ObjectStatus::Created;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitQuadMDC);

	m_unitCubeMDC = addDX11MeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitCube(*m_unitCubeMDC);
	m_unitCubeMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_unitCubeMDC->m_meshShapeType = MeshShapeType::CUBE;
	m_unitCubeMDC->m_objectStatus = ObjectStatus::Created;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitCubeMDC);

	m_unitSphereMDC = addDX11MeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitSphere(*m_unitSphereMDC);
	m_unitSphereMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_unitSphereMDC->m_meshShapeType = MeshShapeType::SPHERE;
	m_unitSphereMDC->m_objectStatus = ObjectStatus::Created;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitSphereMDC);

	m_terrainMDC = addDX11MeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addTerrain(*m_terrainMDC);
	m_terrainMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_terrainMDC->m_objectStatus = ObjectStatus::Created;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_terrainMDC);

	initializeDX11MeshDataComponent(m_unitLineMDC);
	initializeDX11MeshDataComponent(m_unitQuadMDC);
	initializeDX11MeshDataComponent(m_unitCubeMDC);
	initializeDX11MeshDataComponent(m_unitSphereMDC);
	initializeDX11MeshDataComponent(m_terrainMDC);

	initializeDX11TextureDataComponent(m_basicNormalTDC);
	initializeDX11TextureDataComponent(m_basicAlbedoTDC);
	initializeDX11TextureDataComponent(m_basicMetallicTDC);
	initializeDX11TextureDataComponent(m_basicRoughnessTDC);
	initializeDX11TextureDataComponent(m_basicAOTDC);

	initializeDX11TextureDataComponent(m_iconTemplate_OBJ);
	initializeDX11TextureDataComponent(m_iconTemplate_PNG);
	initializeDX11TextureDataComponent(m_iconTemplate_SHADER);
	initializeDX11TextureDataComponent(m_iconTemplate_UNKNOWN);

	initializeDX11TextureDataComponent(m_iconTemplate_DirectionalLight);
	initializeDX11TextureDataComponent(m_iconTemplate_PointLight);
	initializeDX11TextureDataComponent(m_iconTemplate_SphereLight);
}

bool DX11RenderingBackendNS::generateGPUBuffers()
{
	g_DXRenderingBackendComponent->m_cameraConstantBuffer = createConstantBuffer(sizeof(CameraGPUData), 1, "cameraConstantBuffer");
	g_DXRenderingBackendComponent->m_meshConstantBuffer = createConstantBuffer(sizeof(MeshGPUData), RenderingFrontendComponent::get().m_maxMeshes, "meshConstantBuffer");
	g_DXRenderingBackendComponent->m_materialConstantBuffer = createConstantBuffer(sizeof(MaterialGPUData), RenderingFrontendComponent::get().m_maxMaterials, "materialConstantBuffer");
	g_DXRenderingBackendComponent->m_sunConstantBuffer = createConstantBuffer(sizeof(SunGPUData), 1, "sunConstantBuffer");
	g_DXRenderingBackendComponent->m_pointLightConstantBuffer = createConstantBuffer(sizeof(PointLightGPUData), RenderingFrontendComponent::get().m_maxPointLights, "pointLightConstantBuffer");
	g_DXRenderingBackendComponent->m_sphereLightConstantBuffer = createConstantBuffer(sizeof(SphereLightGPUData), RenderingFrontendComponent::get().m_maxSphereLights, "sphereLightConstantBuffer");
	g_DXRenderingBackendComponent->m_skyConstantBuffer = createConstantBuffer(sizeof(SkyGPUData), 1, "skyConstantBuffer");
	g_DXRenderingBackendComponent->m_dispatchParamsConstantBuffer = createConstantBuffer(sizeof(DispatchParamsGPUData), 1, "dispatchParamsConstantBuffer");

	return true;
}

bool DX11RenderingBackendNS::update()
{
	if (DX11RenderingBackendNS::m_uninitializedMDC.size() > 0)
	{
		DX11MeshDataComponent* l_MDC;
		DX11RenderingBackendNS::m_uninitializedMDC.tryPop(l_MDC);

		if (l_MDC)
		{
			auto l_result = initializeDX11MeshDataComponent(l_MDC);
			if (!l_result)
			{
				g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't create DX11MeshDataComponent for " + std::string(l_MDC->m_parentEntity->m_entityName.c_str()) + "!");
			}
		}
	}
	if (DX11RenderingBackendNS::m_uninitializedTDC.size() > 0)
	{
		DX11TextureDataComponent* l_TDC;
		DX11RenderingBackendNS::m_uninitializedTDC.tryPop(l_TDC);

		if (l_TDC)
		{
			auto l_result = initializeDX11TextureDataComponent(l_TDC);
			if (!l_result)
			{
				g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX11RenderingBackend: can't create DX11TextureDataComponent for " + std::string(l_TDC->m_parentEntity->m_entityName.c_str()) + "!");
			}
		}
	}

	return true;
}

bool DX11RenderingBackendNS::render()
{
	updateConstantBuffer(DX11RenderingBackendComponent::get().m_cameraConstantBuffer, RenderingFrontendComponent::get().m_cameraGPUData);
	updateConstantBuffer(DX11RenderingBackendComponent::get().m_sunConstantBuffer, RenderingFrontendComponent::get().m_sunGPUData);
	updateConstantBuffer(DX11RenderingBackendComponent::get().m_pointLightConstantBuffer, RenderingFrontendComponent::get().m_pointLightGPUDataVector);
	updateConstantBuffer(DX11RenderingBackendComponent::get().m_sphereLightConstantBuffer, RenderingFrontendComponent::get().m_sphereLightGPUDataVector);
	updateConstantBuffer(DX11RenderingBackendComponent::get().m_skyConstantBuffer, RenderingFrontendComponent::get().m_skyGPUData);
	updateConstantBuffer(DX11RenderingBackendComponent::get().m_meshConstantBuffer, RenderingFrontendComponent::get().m_opaquePassMeshGPUDatas);
	updateConstantBuffer(DX11RenderingBackendComponent::get().m_materialConstantBuffer, RenderingFrontendComponent::get().m_opaquePassMaterialGPUDatas);

	DX11RenderingBackendComponent::get().m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DX11OpaquePass::update();

	DX11LightCullingPass::update();

	DX11LightPass::update();

	DX11SkyPass::update();

	DX11PreTAAPass::update();

	DX11TAAPass::update();

	DX11PostTAAPass::update();

	DX11MotionBlurPass::update();

	DX11FinalBlendPass::update();

	return true;
}

bool DX11RenderingBackendNS::terminate()
{
	// Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
	if (g_DXRenderingBackendComponent->m_swapChain)
	{
		g_DXRenderingBackendComponent->m_swapChain->SetFullscreenState(false, NULL);
	}

	if (g_DXRenderingBackendComponent->m_deviceContext)
	{
		g_DXRenderingBackendComponent->m_deviceContext->Release();
		g_DXRenderingBackendComponent->m_deviceContext = 0;
	}

	if (g_DXRenderingBackendComponent->m_device)
	{
		g_DXRenderingBackendComponent->m_device->Release();
		g_DXRenderingBackendComponent->m_device = 0;
	}

	if (g_DXRenderingBackendComponent->m_swapChain)
	{
		g_DXRenderingBackendComponent->m_swapChain->Release();
		g_DXRenderingBackendComponent->m_swapChain = 0;
	}

	m_objectStatus = ObjectStatus::Terminated;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX11RenderingBackend has been terminated.");
	return true;
}

DX11MeshDataComponent* DX11RenderingBackendNS::addDX11MeshDataComponent()
{
	static std::atomic<unsigned int> meshCount = 0;
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_MeshDataComponentPool, sizeof(DX11MeshDataComponent));
	auto l_MDC = new(l_rawPtr)DX11MeshDataComponent();
	auto l_parentEntity = g_pCoreSystem->getGameSystem()->createEntity(EntityName(("Mesh_" + std::to_string(meshCount) + "/").c_str()), ObjectSource::Runtime, ObjectUsage::Engine);
	l_MDC->m_parentEntity = l_parentEntity;
	meshCount++;
	return l_MDC;
}

MaterialDataComponent* DX11RenderingBackendNS::addMaterialDataComponent()
{
	static std::atomic<unsigned int> materialCount = 0;
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_MaterialDataComponentPool, sizeof(MaterialDataComponent));
	auto l_MDC = new(l_rawPtr)MaterialDataComponent();
	auto l_parentEntity = g_pCoreSystem->getGameSystem()->createEntity(EntityName(("Material_" + std::to_string(materialCount) + "/").c_str()), ObjectSource::Runtime, ObjectUsage::Engine);
	l_MDC->m_parentEntity = l_parentEntity;
	materialCount++;
	return l_MDC;
}

DX11TextureDataComponent* DX11RenderingBackendNS::addDX11TextureDataComponent()
{
	static std::atomic<unsigned int> textureCount = 0;
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_TextureDataComponentPool, sizeof(DX11TextureDataComponent));
	auto l_TDC = new(l_rawPtr)DX11TextureDataComponent();
	auto l_parentEntity = g_pCoreSystem->getGameSystem()->createEntity(EntityName(("Texture_" + std::to_string(textureCount) + "/").c_str()), ObjectSource::Runtime, ObjectUsage::Engine);
	l_TDC->m_parentEntity = l_parentEntity;
	textureCount++;
	return l_TDC;
}

DX11MeshDataComponent* DX11RenderingBackendNS::getDX11MeshDataComponent(MeshShapeType meshShapeType)
{
	switch (meshShapeType)
	{
	case MeshShapeType::LINE:
		return DX11RenderingBackendNS::m_unitLineMDC; break;
	case MeshShapeType::QUAD:
		return DX11RenderingBackendNS::m_unitQuadMDC; break;
	case MeshShapeType::CUBE:
		return DX11RenderingBackendNS::m_unitCubeMDC; break;
	case MeshShapeType::SPHERE:
		return DX11RenderingBackendNS::m_unitSphereMDC; break;
	case MeshShapeType::TERRAIN:
		return DX11RenderingBackendNS::m_terrainMDC; break;
	case MeshShapeType::CUSTOM:
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackend: wrong MeshShapeType passed to DX11RenderingBackend::getMeshDataComponent() !");
		return nullptr; break;
	default:
		return nullptr; break;
	}
}

DX11TextureDataComponent * DX11RenderingBackendNS::getDX11TextureDataComponent(TextureUsageType textureUsageType)
{
	switch (textureUsageType)
	{
	case TextureUsageType::INVISIBLE:
		return nullptr; break;
	case TextureUsageType::NORMAL:
		return DX11RenderingBackendNS::m_basicNormalTDC; break;
	case TextureUsageType::ALBEDO:
		return DX11RenderingBackendNS::m_basicAlbedoTDC; break;
	case TextureUsageType::METALLIC:
		return DX11RenderingBackendNS::m_basicMetallicTDC; break;
	case TextureUsageType::ROUGHNESS:
		return DX11RenderingBackendNS::m_basicRoughnessTDC; break;
	case TextureUsageType::AMBIENT_OCCLUSION:
		return DX11RenderingBackendNS::m_basicAOTDC; break;
	case TextureUsageType::COLOR_ATTACHMENT:
		return nullptr; break;
	default:
		return nullptr; break;
	}
}

DX11TextureDataComponent * DX11RenderingBackendNS::getDX11TextureDataComponent(FileExplorerIconType iconType)
{
	switch (iconType)
	{
	case FileExplorerIconType::OBJ:
		return DX11RenderingBackendNS::m_iconTemplate_OBJ; break;
	case FileExplorerIconType::PNG:
		return DX11RenderingBackendNS::m_iconTemplate_PNG; break;
	case FileExplorerIconType::SHADER:
		return DX11RenderingBackendNS::m_iconTemplate_SHADER; break;
	case FileExplorerIconType::UNKNOWN:
		return DX11RenderingBackendNS::m_iconTemplate_UNKNOWN; break;
	default:
		return nullptr; break;
	}
}

DX11TextureDataComponent * DX11RenderingBackendNS::getDX11TextureDataComponent(WorldEditorIconType iconType)
{
	switch (iconType)
	{
	case WorldEditorIconType::DIRECTIONAL_LIGHT:
		return DX11RenderingBackendNS::m_iconTemplate_DirectionalLight; break;
	case WorldEditorIconType::POINT_LIGHT:
		return DX11RenderingBackendNS::m_iconTemplate_PointLight; break;
	case WorldEditorIconType::SPHERE_LIGHT:
		return DX11RenderingBackendNS::m_iconTemplate_SphereLight; break;
	default:
		return nullptr; break;
	}
}

bool DX11RenderingBackendNS::resize()
{
	auto l_screenResolution = g_pCoreSystem->getRenderingFrontend()->getScreenResolution();

	g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.Width = l_screenResolution.x;
	g_DXRenderingBackendComponent->m_swapChainDesc.BufferDesc.Height = l_screenResolution.y;

	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.width = l_screenResolution.x;
	g_DXRenderingBackendComponent->m_deferredRenderPassDesc.RTDesc.height = l_screenResolution.y;

	DX11OpaquePass::resize();
	DX11LightCullingPass::resize();
	DX11LightPass::resize();
	DX11SkyPass::resize();
	DX11PreTAAPass::resize();
	DX11TAAPass::resize();
	DX11PostTAAPass::resize();
	DX11MotionBlurPass::resize();
	DX11FinalBlendPass::resize();

	return true;
}

bool DX11RenderingBackend::setup()
{
	return DX11RenderingBackendNS::setup();
}

bool DX11RenderingBackend::initialize()
{
	return DX11RenderingBackendNS::initialize();
}

bool DX11RenderingBackend::update()
{
	return DX11RenderingBackendNS::update();
}

bool DX11RenderingBackend::render()
{
	return DX11RenderingBackendNS::render();
}

bool DX11RenderingBackend::terminate()
{
	return DX11RenderingBackendNS::terminate();
}

ObjectStatus DX11RenderingBackend::getStatus()
{
	return DX11RenderingBackendNS::m_objectStatus;
}

MeshDataComponent * DX11RenderingBackend::addMeshDataComponent()
{
	return DX11RenderingBackendNS::addDX11MeshDataComponent();
}

MaterialDataComponent * DX11RenderingBackend::addMaterialDataComponent()
{
	return DX11RenderingBackendNS::addMaterialDataComponent();
}

TextureDataComponent * DX11RenderingBackend::addTextureDataComponent()
{
	return DX11RenderingBackendNS::addDX11TextureDataComponent();
}

MeshDataComponent * DX11RenderingBackend::getMeshDataComponent(MeshShapeType MeshShapeType)
{
	return DX11RenderingBackendNS::getDX11MeshDataComponent(MeshShapeType);
}

TextureDataComponent * DX11RenderingBackend::getTextureDataComponent(TextureUsageType TextureUsageType)
{
	return DX11RenderingBackendNS::getDX11TextureDataComponent(TextureUsageType);
}

TextureDataComponent * DX11RenderingBackend::getTextureDataComponent(FileExplorerIconType iconType)
{
	return DX11RenderingBackendNS::getDX11TextureDataComponent(iconType);
}

TextureDataComponent * DX11RenderingBackend::getTextureDataComponent(WorldEditorIconType iconType)
{
	return DX11RenderingBackendNS::getDX11TextureDataComponent(iconType);
}

void DX11RenderingBackend::registerUninitializedMeshDataComponent(MeshDataComponent * rhs)
{
	DX11RenderingBackendNS::m_uninitializedMDC.push(reinterpret_cast<DX11MeshDataComponent*>(rhs));
}

void DX11RenderingBackend::registerUninitializedTextureDataComponent(TextureDataComponent * rhs)
{
	DX11RenderingBackendNS::m_uninitializedTDC.push(reinterpret_cast<DX11TextureDataComponent*>(rhs));
}

bool DX11RenderingBackend::resize()
{
	return DX11RenderingBackendNS::resize();
}

bool DX11RenderingBackend::reloadShader(RenderPassType renderPassType)
{
	switch (renderPassType)
	{
	case RenderPassType::Shadow:
		break;
	case RenderPassType::Opaque:
		DX11OpaquePass::reloadShaders();
		break;
	case RenderPassType::Light:
		DX11LightPass::reloadShaders();
		break;
	case RenderPassType::Transparent:
		break;
	case RenderPassType::Terrain:
		break;
	case RenderPassType::PostProcessing:
		DX11SkyPass::reloadShaders();
		DX11PreTAAPass::reloadShaders();
		DX11TAAPass::reloadShaders();
		DX11PostTAAPass::reloadShaders();
		DX11MotionBlurPass::reloadShaders();
		DX11FinalBlendPass::reloadShaders();
		break;
	default: break;
	}

	return true;
}

bool DX11RenderingBackend::bakeGI()
{
	return true;
}