#include "DX11RenderingServer.h"
#include "../../Component/DX11MeshDataComponent.h"
#include "../../Component/DX11TextureDataComponent.h"
#include "../../Component/DX11MaterialDataComponent.h"
#include "../../Component/DX11RenderPassDataComponent.h"
#include "../../Component/DX11ShaderProgramComponent.h"
#include "../../Component/DX11GPUBufferDataComponent.h"
#include "../../Component/WinWindowSystemComponent.h"

#include "DX11Helper.h"

using namespace DX11Helper;

#include "../../ModuleManager/IModuleManager.h"

extern IModuleManager* g_pModuleManager;

#include "../../Core/InnoLogger.h"
#include "../../Core/InnoMemory.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

namespace DX11RenderingServerNS
{
	template <typename U, typename T>
	bool SetObjectName(U* owner, T* rhs, const char* objectType)
	{
		auto l_Name = std::string(owner->m_componentName.c_str());
		l_Name += "_";
		l_Name += objectType;
		auto l_HResult = rhs->SetPrivateData(WKPDID_D3DDebugObjectName, (unsigned int)l_Name.size(), l_Name.c_str());
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Warning, "DX11RenderingServer: Can't name ", objectType, " with ", l_Name.c_str());
			return false;
		}
		return true;
	}

	ObjectStatus m_objectStatus = ObjectStatus::Terminated;

	IObjectPool* m_MeshDataComponentPool;
	IObjectPool* m_MaterialDataComponentPool;
	IObjectPool* m_TextureDataComponentPool;
	IObjectPool* m_RenderPassDataComponentPool;
	IObjectPool* m_PSOPool;
	IObjectPool* m_ShaderProgramComponentPool;

	std::unordered_set<MeshDataComponent*> m_initializedMeshes;
	std::unordered_set<TextureDataComponent*> m_initializedTextures;
	std::unordered_set<MaterialDataComponent*> m_initializedMaterials;

	TVec2<unsigned int> m_refreshRate = TVec2<unsigned int>(0, 1);

	int m_videoCardMemory;
	char m_videoCardDescription[128];

	IDXGIFactory* m_factory;

	DXGI_ADAPTER_DESC m_adapterDesc;
	IDXGIAdapter* m_adapter;
	IDXGIOutput* m_adapterOutput;

	ID3D11Device5* m_device;
	ID3D11DeviceContext4* m_deviceContext;

	DXGI_SWAP_CHAIN_DESC m_swapChainDesc;
	IDXGISwapChain4* m_swapChain;
	std::vector<ID3D11Texture2D*> m_swapChainTextures;

	ID3D10Blob* m_InputLayoutDummyShaderBuffer = 0;
}

using namespace DX11RenderingServerNS;

bool DX11RenderingServer::Setup()
{
	auto l_renderingCapability = g_pModuleManager->getRenderingFrontend()->getRenderingCapability();

	m_MeshDataComponentPool = InnoMemory::CreateObjectPool(sizeof(DX11MeshDataComponent), l_renderingCapability.maxMeshes);
	m_TextureDataComponentPool = InnoMemory::CreateObjectPool(sizeof(DX11TextureDataComponent), l_renderingCapability.maxTextures);
	m_MaterialDataComponentPool = InnoMemory::CreateObjectPool(sizeof(DX11MaterialDataComponent), l_renderingCapability.maxMaterials);
	m_RenderPassDataComponentPool = InnoMemory::CreateObjectPool(sizeof(DX11RenderPassDataComponent), 128);
	m_PSOPool = InnoMemory::CreateObjectPool(sizeof(DX11PipelineStateObject), 128);
	m_ShaderProgramComponentPool = InnoMemory::CreateObjectPool(sizeof(DX11ShaderProgramComponent), 256);

	HRESULT l_HResult;
	unsigned int l_numModes;
	unsigned long long l_stringLength;

	// Create a DirectX graphics interface factory.
	l_HResult = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&m_factory);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't create DXGI factory!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Use the factory to create an adapter for the primary graphics interface (video card).
	l_HResult = m_factory->EnumAdapters(0, &m_adapter);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't create video card adapter!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Enumerate the primary adapter output (monitor).
	l_HResult = m_adapter->EnumOutputs(0, &m_adapterOutput);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't create monitor adapter!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	l_HResult = m_adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &l_numModes, NULL);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't get DXGI_FORMAT_R8G8B8A8_UNORM fitted monitor!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Create a list to hold all the possible display modes for this monitor/video card combination.
	std::vector<DXGI_MODE_DESC> displayModeList(l_numModes);

	// Now fill the display mode list structures.
	l_HResult = m_adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &l_numModes, &displayModeList[0]);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't fill the display mode list structures!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Now go through all the display modes and find the one that matches the screen width and height.
	// When a match is found store the numerator and denominator of the refresh rate for that monitor.
	auto l_screenResolution = g_pModuleManager->getRenderingFrontend()->getScreenResolution();

	for (unsigned int i = 0; i < l_numModes; i++)
	{
		if (displayModeList[i].Width == l_screenResolution.x
			&&
			displayModeList[i].Height == l_screenResolution.y
			)
		{
			m_refreshRate.x = displayModeList[i].RefreshRate.Numerator;
			m_refreshRate.y = displayModeList[i].RefreshRate.Denominator;
		}
	}

	// Get the adapter (video card) description.
	l_HResult = m_adapter->GetDesc(&m_adapterDesc);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't get the video card adapter description!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	// Store the dedicated video card memory in megabytes.
	m_videoCardMemory = (int)(m_adapterDesc.DedicatedVideoMemory / 1024 / 1024);

	// Convert the name of the video card to a character array and store it.
	if (wcstombs_s(&l_stringLength, m_videoCardDescription, 128, m_adapterDesc.Description, 128) != 0)
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't convert the name of the video card to a character array!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	D3D_FEATURE_LEVEL featureLevel;

	// Initialize the swap chain description.
	ZeroMemory(&m_swapChainDesc, sizeof(m_swapChainDesc));

	// Set to a single back buffer.
	m_swapChainDesc.BufferCount = 1;

	// Set the width and height of the back buffer.
	m_swapChainDesc.BufferDesc.Width = l_screenResolution.x;
	m_swapChainDesc.BufferDesc.Height = l_screenResolution.y;

	// Set regular 32-bit surface for the back buffer.
	m_swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Set the refresh rate of the back buffer.
	auto l_renderingConfig = g_pModuleManager->getRenderingFrontend()->getRenderingConfig();

	if (l_renderingConfig.VSync)
	{
		m_swapChainDesc.BufferDesc.RefreshRate.Numerator = m_refreshRate.x;
		m_swapChainDesc.BufferDesc.RefreshRate.Denominator = m_refreshRate.y;
	}
	else
	{
		m_swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		m_swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	}

	// Set the usage of the back buffer.
	m_swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	// Set the handle for the window to render to.
	m_swapChainDesc.OutputWindow = WinWindowSystemComponent::get().m_hwnd;

	// Turn multisampling off.
	m_swapChainDesc.SampleDesc.Count = 1;
	m_swapChainDesc.SampleDesc.Quality = 0;

	// Set to full screen or windowed mode.
	// @TODO: finish this feature
	m_swapChainDesc.Windowed = true;

	// Set the scan line ordering and scaling to unspecified.
	m_swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	m_swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// Discard the back buffer contents after presenting.
	m_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Don't set the advanced flags.
	m_swapChainDesc.Flags = 0;

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

	l_HResult = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, &featureLevel, 1,
		D3D11_SDK_VERSION, &m_swapChainDesc, &l_swapChain, &l_device, NULL, &l_deviceContext);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't create the swap chain/D3D device/D3D device context!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	m_device = reinterpret_cast<ID3D11Device5*>(l_device);
	m_deviceContext = reinterpret_cast<ID3D11DeviceContext4*>(l_deviceContext);
	m_swapChain = reinterpret_cast<IDXGISwapChain4*>(l_swapChain);

	m_swapChainTextures.resize(1);
	// Get the pointer to the back buffer.
	l_HResult = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&m_swapChainTextures[0]);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't get back buffer pointer!");
		m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	m_objectStatus = ObjectStatus::Created;
	InnoLogger::Log(LogLevel::Success, "DX11RenderingServer setup finished.");

	return true;
}

bool DX11RenderingServer::Initialize()
{
	if (m_objectStatus == ObjectStatus::Created)
	{
		// @TODO: Find a better solution
		LoadShaderFile(&m_InputLayoutDummyShaderBuffer, ShaderType::VERTEX, "dummyInputLayout.hlsl/");
	}

	return true;
}

bool DX11RenderingServer::Terminate()
{
	// Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
	m_swapChain->SetFullscreenState(false, NULL);

	m_deviceContext->Release();
	m_deviceContext = 0;

	m_device->Release();
	m_device = 0;

	m_swapChain->Release();
	m_swapChain = 0;

	m_adapterOutput->Release();
	m_adapterOutput = 0;

	m_adapter->Release();
	m_adapter = 0;

	m_factory->Release();
	m_factory = 0;

	m_objectStatus = ObjectStatus::Terminated;
	InnoLogger::Log(LogLevel::Success, "DX11RenderingServer has been terminated.");

	return true;
}

ObjectStatus DX11RenderingServer::GetStatus()
{
	return m_objectStatus;
}

MeshDataComponent * DX11RenderingServer::AddMeshDataComponent(const char * name)
{
	static std::atomic<unsigned int> l_count = 0;
	l_count++;
	auto l_rawPtr = m_MeshDataComponentPool->Spawn();
	auto l_result = new(l_rawPtr)DX11MeshDataComponent();
	std::string l_name;
	if (strcmp(name, ""))
	{
		l_name = name;
	}
	else
	{
		l_name = ("Mesh_" + std::to_string(l_count) + "/");
	}
	auto l_parentEntity = g_pModuleManager->getEntityManager()->Spawn(ObjectSource::Runtime, ObjectUsage::Engine, l_name.c_str());
	l_result->m_parentEntity = l_parentEntity;
	l_result->m_componentName = l_name.c_str();
	return l_result;
}

TextureDataComponent * DX11RenderingServer::AddTextureDataComponent(const char * name)
{
	static std::atomic<unsigned int> l_count = 0;
	l_count++;
	auto l_rawPtr = m_TextureDataComponentPool->Spawn();
	auto l_result = new(l_rawPtr)DX11TextureDataComponent();
	std::string l_name;
	if (strcmp(name, ""))
	{
		l_name = name;
	}
	else
	{
		l_name = ("Texture_" + std::to_string(l_count) + "/");
	}
	auto l_parentEntity = g_pModuleManager->getEntityManager()->Spawn(ObjectSource::Runtime, ObjectUsage::Engine, l_name.c_str());
	l_result->m_parentEntity = l_parentEntity;
	l_result->m_componentName = l_name.c_str();
	return l_result;
}

MaterialDataComponent * DX11RenderingServer::AddMaterialDataComponent(const char * name)
{
	static std::atomic<unsigned int> l_count = 0;
	l_count++;
	auto l_rawPtr = m_MaterialDataComponentPool->Spawn();
	auto l_result = new(l_rawPtr)DX11MaterialDataComponent();
	std::string l_name;
	if (strcmp(name, ""))
	{
		l_name = name;
	}
	else
	{
		l_name = ("Material_" + std::to_string(l_count) + "/");
	}
	auto l_parentEntity = g_pModuleManager->getEntityManager()->Spawn(ObjectSource::Runtime, ObjectUsage::Engine, l_name.c_str());
	l_result->m_parentEntity = l_parentEntity;
	l_result->m_componentName = l_name.c_str();
	return l_result;
}

RenderPassDataComponent * DX11RenderingServer::AddRenderPassDataComponent(const char * name)
{
	static std::atomic<unsigned int> l_count = 0;
	l_count++;
	auto l_rawPtr = m_RenderPassDataComponentPool->Spawn();
	auto l_result = new(l_rawPtr)DX11RenderPassDataComponent();
	std::string l_name;
	if (strcmp(name, ""))
	{
		l_name = name;
	}
	else
	{
		l_name = ("RenderPass_" + std::to_string(l_count) + "/");
	}
	auto l_parentEntity = g_pModuleManager->getEntityManager()->Spawn(ObjectSource::Runtime, ObjectUsage::Engine, l_name.c_str());
	l_result->m_parentEntity = l_parentEntity;
	l_result->m_componentName = l_name.c_str();
	return l_result;
}

ShaderProgramComponent * DX11RenderingServer::AddShaderProgramComponent(const char * name)
{
	static std::atomic<unsigned int> l_count = 0;
	l_count++;
	auto l_rawPtr = m_ShaderProgramComponentPool->Spawn();
	auto l_result = new(l_rawPtr)DX11ShaderProgramComponent();
	std::string l_name;
	if (strcmp(name, ""))
	{
		l_name = name;
	}
	else
	{
		l_name = ("ShaderProgram_" + std::to_string(l_count) + "/");
	}
	auto l_parentEntity = g_pModuleManager->getEntityManager()->Spawn(ObjectSource::Runtime, ObjectUsage::Engine, l_name.c_str());
	l_result->m_parentEntity = l_parentEntity;
	l_result->m_componentName = l_name.c_str();
	return l_result;
}

GPUBufferDataComponent * DX11RenderingServer::AddGPUBufferDataComponent(const char * name)
{
	static std::atomic<unsigned int> l_count = 0;
	l_count++;
	auto l_rawPtr = m_ShaderProgramComponentPool->Spawn();
	auto l_result = new(l_rawPtr)DX11GPUBufferDataComponent();
	std::string l_name;
	if (strcmp(name, ""))
	{
		l_name = name;
	}
	else
	{
		l_name = ("GPUBufferData_" + std::to_string(l_count) + "/");
	}
	auto l_parentEntity = g_pModuleManager->getEntityManager()->Spawn(ObjectSource::Runtime, ObjectUsage::Engine, l_name.c_str());
	l_result->m_parentEntity = l_parentEntity;
	l_result->m_componentName = l_name.c_str();
	return l_result;
}

bool DX11RenderingServer::InitializeMeshDataComponent(MeshDataComponent * rhs)
{
	if (m_initializedMeshes.find(rhs) != m_initializedMeshes.end())
	{
		return true;
	}

	auto l_rhs = reinterpret_cast<DX11MeshDataComponent*>(rhs);

	// Set up the description of the static vertex buffer.
	D3D11_BUFFER_DESC l_vertexBufferDesc;
	ZeroMemory(&l_vertexBufferDesc, sizeof(l_vertexBufferDesc));
	l_vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	l_vertexBufferDesc.ByteWidth = sizeof(Vertex) * (UINT)l_rhs->m_vertices.size();
	l_vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	l_vertexBufferDesc.CPUAccessFlags = 0;
	l_vertexBufferDesc.MiscFlags = 0;
	l_vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	D3D11_SUBRESOURCE_DATA l_vertexSubresourceData;
	ZeroMemory(&l_vertexSubresourceData, sizeof(l_vertexSubresourceData));
	l_vertexSubresourceData.pSysMem = &l_rhs->m_vertices[0];
	l_vertexSubresourceData.SysMemPitch = 0;
	l_vertexSubresourceData.SysMemSlicePitch = 0;

	// Now create the vertex buffer.
	HRESULT l_HResult;
	l_HResult = m_device->CreateBuffer(&l_vertexBufferDesc, &l_vertexSubresourceData, &l_rhs->m_vertexBuffer);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't create Vertex Buffer!");
		return false;
	}
#ifdef  _DEBUG
	SetObjectName(l_rhs, l_rhs->m_vertexBuffer, "VB");
#endif //  _DEBUG

	InnoLogger::Log(LogLevel::Verbose, "DX11RenderingServer: Vertex Buffer: ", l_rhs->m_vertexBuffer, " is initialized.");

	// Set up the description of the static index buffer.
	D3D11_BUFFER_DESC l_indexBufferDesc;
	ZeroMemory(&l_indexBufferDesc, sizeof(l_indexBufferDesc));
	l_indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	l_indexBufferDesc.ByteWidth = (UINT)(l_rhs->m_indices.size() * sizeof(unsigned int));
	l_indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	l_indexBufferDesc.CPUAccessFlags = 0;
	l_indexBufferDesc.MiscFlags = 0;
	l_indexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the index data.
	D3D11_SUBRESOURCE_DATA l_indexSubresourceData;
	ZeroMemory(&l_indexSubresourceData, sizeof(l_indexSubresourceData));
	l_indexSubresourceData.pSysMem = &l_rhs->m_indices[0];
	l_indexSubresourceData.SysMemPitch = 0;
	l_indexSubresourceData.SysMemSlicePitch = 0;

	// Create the index buffer.
	l_HResult = m_device->CreateBuffer(&l_indexBufferDesc, &l_indexSubresourceData, &l_rhs->m_indexBuffer);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't create Index Buffer!");
		return false;
	}
#ifdef  _DEBUG
	SetObjectName(l_rhs, l_rhs->m_indexBuffer, "IB");
#endif //  _DEBUG

	InnoLogger::Log(LogLevel::Verbose, "DX11RenderingServer: Index Buffer: ", l_rhs->m_indexBuffer, " is initialized.");

	l_rhs->m_objectStatus = ObjectStatus::Activated;

	m_initializedMeshes.emplace(l_rhs);

	return true;
}

bool DX11RenderingServer::InitializeTextureDataComponent(TextureDataComponent * rhs)
{
	if (m_initializedTextures.find(rhs) != m_initializedTextures.end())
	{
		return true;
	}

	auto l_rhs = reinterpret_cast<DX11TextureDataComponent*>(rhs);

	l_rhs->m_DX11TextureDataDesc = GetDX11TextureDataDesc(l_rhs->m_textureDataDesc);

	// Create the empty texture.
	HRESULT l_HResult;

	if (l_rhs->m_textureDataDesc.samplerType == TextureSamplerType::SAMPLER_1D)
	{
		auto l_desc = Get1DTextureDataDesc(l_rhs->m_DX11TextureDataDesc);
		l_HResult = m_device->CreateTexture1D(&l_desc, NULL, (ID3D11Texture1D**)&l_rhs->m_ResourceHandle);
	}
	else if (l_rhs->m_textureDataDesc.samplerType == TextureSamplerType::SAMPLER_2D)
	{
		auto l_desc = Get2DTextureDataDesc(l_rhs->m_DX11TextureDataDesc);
		l_HResult = m_device->CreateTexture2D(&l_desc, NULL, (ID3D11Texture2D**)&l_rhs->m_ResourceHandle);
	}
	else if (l_rhs->m_textureDataDesc.samplerType == TextureSamplerType::SAMPLER_3D)
	{
		auto l_desc = Get3DTextureDataDesc(l_rhs->m_DX11TextureDataDesc);
		l_HResult = m_device->CreateTexture3D(&l_desc, NULL, (ID3D11Texture3D**)&l_rhs->m_ResourceHandle);
	}
	else
	{
		// @TODO: Cubemap support
	}

	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't create Texture!");
		return false;
	}

	// Submit raw data to GPU memory
	if (l_rhs->m_textureDataDesc.usageType != TextureUsageType::COLOR_ATTACHMENT
		&& l_rhs->m_textureDataDesc.usageType != TextureUsageType::DEPTH_ATTACHMENT
		&& l_rhs->m_textureDataDesc.usageType != TextureUsageType::DEPTH_STENCIL_ATTACHMENT
		&& l_rhs->m_textureDataDesc.usageType != TextureUsageType::RAW_IMAGE)
	{
		UINT l_rowPitch = (l_rhs->m_textureDataDesc.width * ((unsigned int)l_rhs->m_textureDataDesc.pixelDataFormat + 1)) * sizeof(unsigned char);

		if (l_rhs->m_textureDataDesc.samplerType == TextureSamplerType::SAMPLER_3D)
		{
			UINT l_depthPitch = l_rowPitch * l_rhs->m_textureDataDesc.height;
			m_deviceContext->UpdateSubresource(l_rhs->m_ResourceHandle, 0, NULL, l_rhs->m_textureData, l_rowPitch, l_depthPitch);
		}
		else
		{
			m_deviceContext->UpdateSubresource(l_rhs->m_ResourceHandle, 0, NULL, l_rhs->m_textureData, l_rowPitch, 0);
		}
	}
#ifdef  _DEBUG
	SetObjectName(l_rhs, l_rhs->m_ResourceHandle, "Texture");
#endif //  _DEBUG

	InnoLogger::Log(LogLevel::Verbose, "DX11RenderingServer: Texture: ", l_rhs->m_ResourceHandle, " is initialized.");

	// Create SRV
	l_rhs->m_SRVDesc = GetSRVDesc(l_rhs->m_textureDataDesc, l_rhs->m_DX11TextureDataDesc);

	l_HResult = m_device->CreateShaderResourceView(l_rhs->m_ResourceHandle, &l_rhs->m_SRVDesc, &l_rhs->m_SRV);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't create SRV for texture!");
		return false;
	}
#ifdef  _DEBUG
	SetObjectName(l_rhs, l_rhs->m_SRV, "SRV");
#endif //  _DEBUG

	InnoLogger::Log(LogLevel::Verbose, "DX11RenderingServer: SRV: ", l_rhs->m_SRV, " is initialized.");

	// Generate mipmaps for this texture.
	if (l_rhs->m_textureDataDesc.magFilterMethod == TextureFilterMethod::LINEAR_MIPMAP_LINEAR)
	{
		m_deviceContext->GenerateMips(l_rhs->m_SRV);
	}

	// Create UAV
	if (l_rhs->m_textureDataDesc.usageType == TextureUsageType::RAW_IMAGE)
	{
		l_rhs->m_UAVDesc = GetUAVDesc(l_rhs->m_textureDataDesc, l_rhs->m_DX11TextureDataDesc);

		l_HResult = m_device->CreateUnorderedAccessView(l_rhs->m_ResourceHandle, &l_rhs->m_UAVDesc, &l_rhs->m_UAV);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: Can't create UAV for texture!");
			return false;
		}
#ifdef  _DEBUG
		SetObjectName(l_rhs, l_rhs->m_UAV, "UAV");
#endif //  _DEBUG

		InnoLogger::Log(LogLevel::Verbose, "DX11RenderingServer: UAV: ", l_rhs->m_SRV, " is initialized.");
	}

	l_rhs->m_objectStatus = ObjectStatus::Activated;

	m_initializedTextures.emplace(l_rhs);

	return true;
}

bool DX11RenderingServer::InitializeMaterialDataComponent(MaterialDataComponent * rhs)
{
	if (m_initializedMaterials.find(rhs) != m_initializedMaterials.end())
	{
		return true;
	}

	if (rhs->m_normalTexture)
	{
		InitializeTextureDataComponent(rhs->m_normalTexture);
	}
	if (rhs->m_albedoTexture)
	{
		InitializeTextureDataComponent(rhs->m_albedoTexture);
	}
	if (rhs->m_metallicTexture)
	{
		InitializeTextureDataComponent(rhs->m_metallicTexture);
	}
	if (rhs->m_roughnessTexture)
	{
		InitializeTextureDataComponent(rhs->m_roughnessTexture);
	}
	if (rhs->m_aoTexture)
	{
		InitializeTextureDataComponent(rhs->m_aoTexture);
	}

	rhs->m_objectStatus = ObjectStatus::Activated;

	m_initializedMaterials.emplace(rhs);

	return true;
}

bool DX11RenderingServer::InitializeRenderPassDataComponent(RenderPassDataComponent * rhs)
{
	auto l_rhs = reinterpret_cast<DX11RenderPassDataComponent*>(rhs);

	// RT
	l_rhs->m_RenderTargets.reserve(l_rhs->m_RenderPassDesc.m_RenderTargetCount);
	for (size_t i = 0; i < l_rhs->m_RenderPassDesc.m_RenderTargetCount; i++)
	{
		l_rhs->m_RenderTargets.emplace_back();
	}

	for (size_t i = 0; i < l_rhs->m_RenderPassDesc.m_RenderTargetCount; i++)
	{
		l_rhs->m_RenderTargets[i] = AddTextureDataComponent((std::string(l_rhs->m_componentName.c_str()) + "_" + std::to_string(i) + "/").c_str());
	}

	for (unsigned int i = 0; i < l_rhs->m_RenderPassDesc.m_RenderTargetCount; i++)
	{
		auto l_TDC = l_rhs->m_RenderTargets[i];

		l_TDC->m_textureDataDesc = l_rhs->m_RenderPassDesc.m_RenderTargetDesc;

		l_TDC->m_textureData = nullptr;

		InitializeTextureDataComponent(l_TDC);
	}

	if (l_rhs->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_UseDepthBuffer)
	{
		l_rhs->m_DepthStencilRenderTarget = AddTextureDataComponent((std::string(l_rhs->m_componentName.c_str()) + "_DS/").c_str());
		l_rhs->m_DepthStencilRenderTarget->m_textureDataDesc = l_rhs->m_RenderPassDesc.m_RenderTargetDesc;

		if (l_rhs->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_UseStencilBuffer)
		{
			l_rhs->m_DepthStencilRenderTarget->m_textureDataDesc.usageType = TextureUsageType::DEPTH_STENCIL_ATTACHMENT;
		}
		else
		{
			l_rhs->m_DepthStencilRenderTarget->m_textureDataDesc.usageType = TextureUsageType::DEPTH_ATTACHMENT;
		}

		l_rhs->m_DepthStencilRenderTarget->m_textureData = nullptr;

		InitializeTextureDataComponent(l_rhs->m_DepthStencilRenderTarget);
	}

	// RTV
	l_rhs->m_RTVs.reserve(l_rhs->m_RenderPassDesc.m_RenderTargetCount);
	for (size_t i = 0; i < l_rhs->m_RenderPassDesc.m_RenderTargetCount; i++)
	{
		l_rhs->m_RTVs.emplace_back();
	}

	l_rhs->m_RTVDesc = GetRTVDesc(l_rhs->m_RenderPassDesc.m_RenderTargetDesc);

	for (unsigned int i = 0; i < l_rhs->m_RenderPassDesc.m_RenderTargetCount; i++)
	{
		auto l_DXTDC = reinterpret_cast<DX11TextureDataComponent*>(l_rhs->m_RenderTargets[i]);

		auto l_HResult = m_device->CreateRenderTargetView(l_DXTDC->m_ResourceHandle, &l_rhs->m_RTVDesc, &l_rhs->m_RTVs[i]);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create RTV for ", l_rhs->m_componentName.c_str(), "!");
			return false;
		}
#ifdef  _DEBUG
		auto l_RTVName = "RTV_" + std::to_string(i);
		SetObjectName(l_rhs, l_rhs->m_RTVs[i], l_RTVName.c_str());
#endif //  _DEBUG
	}

	// DSV
	if (l_rhs->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_UseDepthBuffer)
	{
		l_rhs->m_DSVDesc = GetDSVDesc(l_rhs->m_RenderPassDesc.m_RenderTargetDesc, l_rhs->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc);

		auto l_DXTDC = reinterpret_cast<DX11TextureDataComponent*>(l_rhs->m_DepthStencilRenderTarget);

		auto l_HResult = m_device->CreateDepthStencilView(l_DXTDC->m_ResourceHandle, &l_rhs->m_DSVDesc, &l_rhs->m_DSV);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create the DSV for ", l_rhs->m_componentName.c_str(), "!");
			return false;
		}
#ifdef  _DEBUG
		SetObjectName(l_rhs, l_rhs->m_DSV, "DSV");
#endif //  _DEBUG
	}

	// PSO
	auto l_PSORawPtr = m_PSOPool->Spawn();
	auto l_PSO = new(l_PSORawPtr)DX11PipelineStateObject();

	GenerateDepthStencilStateDesc(l_rhs->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc, l_PSO);
	GenerateBlendStateDesc(l_rhs->m_RenderPassDesc.m_GraphicsPipelineDesc.m_BlendDesc, l_PSO);
	GenerateRasterizerStateDesc(l_rhs->m_RenderPassDesc.m_GraphicsPipelineDesc.m_RasterizerDesc, l_PSO);
	GenerateViewportStateDesc(l_rhs->m_RenderPassDesc.m_GraphicsPipelineDesc.m_ViewportDesc, l_PSO);

	// Input layout object
	D3D11_INPUT_ELEMENT_DESC l_inputLayouts[5];

	l_inputLayouts[0].SemanticName = "POSITION";
	l_inputLayouts[0].SemanticIndex = 0;
	l_inputLayouts[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	l_inputLayouts[0].InputSlot = 0;
	l_inputLayouts[0].AlignedByteOffset = 0;
	l_inputLayouts[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_inputLayouts[0].InstanceDataStepRate = 0;

	l_inputLayouts[1].SemanticName = "TEXCOORD";
	l_inputLayouts[1].SemanticIndex = 0;
	l_inputLayouts[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	l_inputLayouts[1].InputSlot = 0;
	l_inputLayouts[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	l_inputLayouts[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_inputLayouts[1].InstanceDataStepRate = 0;

	l_inputLayouts[2].SemanticName = "PADA";
	l_inputLayouts[2].SemanticIndex = 0;
	l_inputLayouts[2].Format = DXGI_FORMAT_R32G32_FLOAT;
	l_inputLayouts[2].InputSlot = 0;
	l_inputLayouts[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	l_inputLayouts[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_inputLayouts[2].InstanceDataStepRate = 0;

	l_inputLayouts[3].SemanticName = "NORMAL";
	l_inputLayouts[3].SemanticIndex = 0;
	l_inputLayouts[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	l_inputLayouts[3].InputSlot = 0;
	l_inputLayouts[3].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	l_inputLayouts[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_inputLayouts[3].InstanceDataStepRate = 0;

	l_inputLayouts[4].SemanticName = "PADB";
	l_inputLayouts[4].SemanticIndex = 0;
	l_inputLayouts[4].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	l_inputLayouts[4].InputSlot = 0;
	l_inputLayouts[4].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	l_inputLayouts[4].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_inputLayouts[4].InstanceDataStepRate = 0;

	auto l_HResult = m_device->CreateInputLayout(l_inputLayouts, 5, m_InputLayoutDummyShaderBuffer->GetBufferPointer(), m_InputLayoutDummyShaderBuffer->GetBufferSize(), &l_PSO->m_InputLayout);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create input layout object!");
		return false;
	}
#ifdef  _DEBUG
	SetObjectName(l_rhs, l_PSO->m_InputLayout, "ILO");
#endif //  _DEBUG

	// Sampler state object
	l_HResult = m_device->CreateSamplerState(&l_PSO->m_SamplerDesc, &l_PSO->m_SamplerState);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create sampler state object for ", l_rhs->m_componentName.c_str(), "!");
		return false;
	}
#ifdef  _DEBUG
	SetObjectName(l_rhs, l_PSO->m_DepthStencilState, "SSO");
#endif //  _DEBUG

	// Depth stencil state object
	if (l_rhs->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_UseDepthBuffer)
	{
		auto l_HResult = m_device->CreateDepthStencilState(&l_PSO->m_DepthStencilDesc, &l_PSO->m_DepthStencilState);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create the depth stencil state object for ", l_rhs->m_componentName.c_str(), "!");
			return false;
		}
#ifdef  _DEBUG
		SetObjectName(l_rhs, l_PSO->m_DepthStencilState, "DSSO");
#endif //  _DEBUG
	}

	// Blend state object
	if (l_rhs->m_RenderPassDesc.m_GraphicsPipelineDesc.m_BlendDesc.m_UseBlend)
	{
		auto l_HResult = m_device->CreateBlendState(&l_PSO->m_BlendDesc, &l_PSO->m_BlendState);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create the blend state object for ", l_rhs->m_componentName.c_str(), "!");
			return false;
		}
#ifdef  _DEBUG
		SetObjectName(l_rhs, l_PSO->m_BlendState, "BSO");
#endif //  _DEBUG
	}

	// Rasterizer state object
	l_HResult = m_device->CreateRasterizerState(&l_PSO->m_RasterizerDesc, &l_PSO->m_RasterizerState);
	if (FAILED(l_HResult))
	{
		InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create the rasterizer state object for ", l_rhs->m_componentName.c_str(), "!");
		return false;
	}
#ifdef  _DEBUG
	SetObjectName(l_rhs, l_PSO->m_RasterizerState, "RSO");
#endif //  _DEBUG

	l_rhs->m_PipelineStateObject = l_PSO;

	l_rhs->m_objectStatus = ObjectStatus::Activated;

	return true;
}

bool DX11RenderingServer::InitializeShaderProgramComponent(ShaderProgramComponent * rhs)
{
	auto l_rhs = reinterpret_cast<DX11ShaderProgramComponent*>(rhs);

	if (l_rhs->m_ShaderFilePaths.m_VSPath != "")
	{
		ID3D10Blob* l_shaderFileBuffer = 0;
		LoadShaderFile(&l_shaderFileBuffer, ShaderType::VERTEX, l_rhs->m_ShaderFilePaths.m_VSPath);
		auto l_HResult = m_device->CreateVertexShader(l_shaderFileBuffer->GetBufferPointer(), l_shaderFileBuffer->GetBufferSize(), NULL, &l_rhs->m_VSHandle);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create vertex shader!");
			return false;
		};
	}
	if (l_rhs->m_ShaderFilePaths.m_TCSPath != "")
	{
		ID3D10Blob* l_shaderFileBuffer = 0;
		LoadShaderFile(&l_shaderFileBuffer, ShaderType::TCS, l_rhs->m_ShaderFilePaths.m_TCSPath);
		auto l_HResult = m_device->CreateHullShader(l_shaderFileBuffer->GetBufferPointer(), l_shaderFileBuffer->GetBufferSize(), NULL, &l_rhs->m_TCSHandle);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create TCS shader!");
			return false;
		};
	}
	if (l_rhs->m_ShaderFilePaths.m_TESPath != "")
	{
		ID3D10Blob* l_shaderFileBuffer = 0;
		LoadShaderFile(&l_shaderFileBuffer, ShaderType::TES, l_rhs->m_ShaderFilePaths.m_TESPath);
		auto l_HResult = m_device->CreateDomainShader(l_shaderFileBuffer->GetBufferPointer(), l_shaderFileBuffer->GetBufferSize(), NULL, &l_rhs->m_TESHandle);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create TES shader!");
			return false;
		};
	}
	if (l_rhs->m_ShaderFilePaths.m_GSPath != "")
	{
		ID3D10Blob* l_shaderFileBuffer = 0;
		LoadShaderFile(&l_shaderFileBuffer, ShaderType::GEOMETRY, l_rhs->m_ShaderFilePaths.m_GSPath);
		auto l_HResult = m_device->CreateGeometryShader(l_shaderFileBuffer->GetBufferPointer(), l_shaderFileBuffer->GetBufferSize(), NULL, &l_rhs->m_GSHandle);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create geometry shader!");
			return false;
		};
	}
	if (l_rhs->m_ShaderFilePaths.m_FSPath != "")
	{
		ID3D10Blob* l_shaderFileBuffer = 0;
		LoadShaderFile(&l_shaderFileBuffer, ShaderType::FRAGMENT, l_rhs->m_ShaderFilePaths.m_FSPath);
		auto l_HResult = m_device->CreatePixelShader(l_shaderFileBuffer->GetBufferPointer(), l_shaderFileBuffer->GetBufferSize(), NULL, &l_rhs->m_FSHandle);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create fragment shader!");
			return false;
		};
	}
	if (l_rhs->m_ShaderFilePaths.m_CSPath != "")
	{
		ID3D10Blob* l_shaderFileBuffer = 0;
		LoadShaderFile(&l_shaderFileBuffer, ShaderType::COMPUTE, l_rhs->m_ShaderFilePaths.m_CSPath);
		auto l_HResult = m_device->CreateComputeShader(l_shaderFileBuffer->GetBufferPointer(), l_shaderFileBuffer->GetBufferSize(), NULL, &l_rhs->m_CSHandle);
		if (FAILED(l_HResult))
		{
			InnoLogger::Log(LogLevel::Error, "DX11RenderingServer: can't create compute shader!");
			return false;
		};
	}

	return true;
}

bool DX11RenderingServer::InitializeGPUBufferDataComponent(GPUBufferDataComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::DeleteMeshDataComponent(MeshDataComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::DeleteTextureDataComponent(TextureDataComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::DeleteMaterialDataComponent(MaterialDataComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::DeleteRenderPassDataComponent(RenderPassDataComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::DeleteShaderProgramComponent(ShaderProgramComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::DeleteGPUBufferDataComponent(GPUBufferDataComponent * rhs)
{
	return false;
}

bool DX11RenderingServer::UploadGPUBufferDataComponentImpl(GPUBufferDataComponent * rhs, const void * GPUBufferValue)
{
	return true;
}

bool DX11RenderingServer::CommandListBegin(RenderPassDataComponent * rhs, size_t frameIndex)
{
	return true;
}

bool DX11RenderingServer::BindRenderPassDataComponent(RenderPassDataComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::CleanRenderTargets(RenderPassDataComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::BindGPUBufferDataComponent(ShaderType shaderType, GPUBufferAccessibility accessibility, GPUBufferDataComponent * GPUBufferDataComponent, size_t startOffset, size_t range)
{
	return true;
}

bool DX11RenderingServer::BindShaderProgramComponent(ShaderProgramComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::BindMaterialDataComponent(MaterialDataComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::DispatchDrawCall(RenderPassDataComponent* renderPass, MeshDataComponent* mesh)
{
	return true;
}

bool DX11RenderingServer::UnbindMaterialDataComponent(MaterialDataComponent * rhs)
{
	return true;
}

bool DX11RenderingServer::CommandListEnd(RenderPassDataComponent * rhs, size_t frameIndex)
{
	return true;
}

bool DX11RenderingServer::ExecuteCommandList(RenderPassDataComponent * rhs, size_t frameIndex)
{
	return true;
}

bool DX11RenderingServer::WaitForFrame(RenderPassDataComponent * rhs, size_t frameIndex)
{
	return true;
}

bool DX11RenderingServer::Present()
{
	auto l_renderingConfig = g_pModuleManager->getRenderingFrontend()->getRenderingConfig();

	if (l_renderingConfig.VSync)
	{
		m_swapChain->Present(1, 0);
	}
	else
	{
		m_swapChain->Present(0, 0);
	}

	return true;
}

bool DX11RenderingServer::CopyDepthBuffer(RenderPassDataComponent * src, RenderPassDataComponent * dest)
{
	return true;
}

bool DX11RenderingServer::CopyStencilBuffer(RenderPassDataComponent * src, RenderPassDataComponent * dest)
{
	return true;
}

bool DX11RenderingServer::CopyColorBuffer(RenderPassDataComponent * src, size_t srcIndex, RenderPassDataComponent * dest, size_t destIndex)
{
	return true;
}

vec4 DX11RenderingServer::ReadRenderTargetSample(RenderPassDataComponent * rhs, size_t renderTargetIndex, size_t x, size_t y)
{
	return vec4();
}

std::vector<vec4> DX11RenderingServer::ReadTextureBackToCPU(RenderPassDataComponent * canvas, TextureDataComponent * TDC)
{
	return std::vector<vec4>();
}

bool DX11RenderingServer::Resize()
{
	return true;
}

bool DX11RenderingServer::ReloadShader(RenderPassType renderPassType)
{
	return true;
}

bool DX11RenderingServer::BakeGIData()
{
	return true;
}