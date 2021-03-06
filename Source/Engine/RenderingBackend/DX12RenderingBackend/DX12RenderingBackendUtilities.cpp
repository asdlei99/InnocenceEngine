#include "DX12RenderingBackendUtilities.h"

#include "../../Component/WinWindowSystemComponent.h"
#include "../../Component/DX12RenderingBackendComponent.h"

#include "../../ModuleManager/IModuleManager.h"

extern IModuleManager* g_pModuleManager;

namespace DX12RenderingBackendNS
{
	ID3D12GraphicsCommandList* beginSingleTimeCommands();
	void endSingleTimeCommands(ID3D12GraphicsCommandList* commandList);
	ID3D12Resource* createUploadHeapBuffer(UINT64 size);
	ID3D12Resource* createDefaultHeapBuffer(D3D12_RESOURCE_DESC* resourceDesc, D3D12_CLEAR_VALUE* clearValue = nullptr);

	void OutputShaderErrorMessage(ID3DBlob * errorMessage, HWND hwnd, const std::string & shaderFilename);
	ID3DBlob* loadShaderBuffer(ShaderType shaderType, const std::wstring & shaderFilePath);

	bool initializeVertexShader(DX12ShaderProgramComponent* rhs, const std::wstring& VSShaderPath);
	bool initializePixelShader(DX12ShaderProgramComponent* rhs, const std::wstring& PSShaderPath);
	bool createSampler(DX12ShaderProgramComponent* rhs);

	bool submitGPUData(DX12MeshDataComponent* rhs);

	D3D12_RESOURCE_DESC getDX12TextureDataDesc(TextureDataDesc textureDataDesc);
	DXGI_FORMAT getTextureFormat(TextureDataDesc textureDataDesc);
	uint32_t getTextureMipLevels(TextureDataDesc textureDataDesc);
	D3D12_RESOURCE_FLAGS getTextureBindFlags(TextureDataDesc textureDataDesc);

	bool submitGPUData(DX12TextureDataComponent* rhs);

	bool submitGPUData(DX12MaterialDataComponent * rhs);

	std::unordered_map<InnoEntity*, DX12MeshDataComponent*> m_initializedDX12Meshes;
	std::unordered_map<InnoEntity*, DX12TextureDataComponent*> m_initializedDX12Textures;
	std::unordered_map<InnoEntity*, DX12MaterialDataComponent*> m_initializedDX12Materials;

	void* m_DX12RenderPassComponentPool;
	void* m_DX12ShaderProgramComponentPool;

	const std::wstring m_shaderRelativePath = L"//Res//Shaders//";
}

bool DX12RenderingBackendNS::initializeComponentPool()
{
	m_DX12RenderPassComponentPool = g_pModuleManager->getMemorySystem()->allocateMemoryPool(sizeof(DX12RenderPassComponent), 128);
	m_DX12ShaderProgramComponentPool = g_pModuleManager->getMemorySystem()->allocateMemoryPool(sizeof(DX12ShaderProgramComponent), 256);

	return true;
}

ID3DBlob* DX12RenderingBackendNS::loadShaderBuffer(ShaderType shaderType, const std::wstring & shaderFilePath)
{
	auto l_shaderName = std::string(shaderFilePath.begin(), shaderFilePath.end());
	std::reverse(l_shaderName.begin(), l_shaderName.end());
	l_shaderName = l_shaderName.substr(l_shaderName.find(".") + 1, l_shaderName.find("//") - l_shaderName.find(".") - 1);
	std::reverse(l_shaderName.begin(), l_shaderName.end());

	HRESULT l_result;
	ID3DBlob* l_errorMessage;
	ID3DBlob* l_shaderBuffer;

	std::string l_shaderTypeName;

	switch (shaderType)
	{
	case ShaderType::VERTEX:
		l_shaderTypeName = "vs_5_1";
		break;
	case ShaderType::GEOMETRY:
		l_shaderTypeName = "gs_5_1";
		break;
	case ShaderType::FRAGMENT:
		l_shaderTypeName = "ps_5_1";
		break;
	default:
		break;
	}

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	auto l_workingDir = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_workingDirW = std::wstring(l_workingDir.begin(), l_workingDir.end());

	l_result = D3DCompileFromFile((l_workingDirW + m_shaderRelativePath + shaderFilePath).c_str(), NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", l_shaderTypeName.c_str(), compileFlags, 0,
		&l_shaderBuffer, &l_errorMessage);
	if (FAILED(l_result))
	{
		// If the shader failed to compile it should have writen something to the error message.
		if (l_errorMessage)
		{
			OutputShaderErrorMessage(l_errorMessage, WinWindowSystemComponent::get().m_hwnd, l_shaderName.c_str());
		}
		// If there was nothing in the error message then it simply could not find the shader file itself.
		else
		{
			MessageBox(WinWindowSystemComponent::get().m_hwnd, l_shaderName.c_str(), "Missing Shader File", MB_OK);
			g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "Shader creation failed: cannot find shader!");
		}

		return nullptr;
	}
	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_VERBOSE, "DX12RenderingBackend: innoShader: " + l_shaderName + " Shader has been compiled.");
	return l_shaderBuffer;
}

DX12ConstantBuffer DX12RenderingBackendNS::createConstantBuffer(size_t elementSize, size_t elementCount, const std::wstring& name)
{
	DX12ConstantBuffer l_result;
	l_result.elementSize = elementSize;
	l_result.elementCount = elementCount;

	// Create ConstantBuffer
	auto l_hResult = DX12RenderingBackendComponent::get().m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(l_result.elementSize * l_result.elementCount),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&l_result.m_constantBuffer));

	if (FAILED(l_hResult))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: can't create ConstantBuffer!");
	}

	// Map ConstantBuffer
	CD3DX12_RANGE constantBufferReadRange(0, 0);
	l_result.m_constantBuffer->Map(0, &constantBufferReadRange, &l_result.mappedMemory);
	l_result.m_constantBuffer->SetName(name.c_str());

	return l_result;
}

DX12CBV DX12RenderingBackendNS::createCBV(const DX12ConstantBuffer& arg, size_t offset)
{
	DX12CBV l_result = {};
	l_result.CBVDesc.BufferLocation = arg.m_constantBuffer->GetGPUVirtualAddress() + offset * arg.elementSize;
	l_result.CBVDesc.SizeInBytes = (uint32_t)arg.elementSize;

	l_result.CPUHandle = DX12RenderingBackendComponent::get().m_currentCSUCPUHandle;
	l_result.GPUHandle = DX12RenderingBackendComponent::get().m_currentCSUGPUHandle;

	DX12RenderingBackendComponent::get().m_device->CreateConstantBufferView(&l_result.CBVDesc, l_result.CPUHandle);

	auto l_CSUDescSize = DX12RenderingBackendComponent::get().m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	DX12RenderingBackendComponent::get().m_currentCSUCPUHandle.ptr += l_CSUDescSize;
	DX12RenderingBackendComponent::get().m_currentCSUGPUHandle.ptr += l_CSUDescSize;

	return l_result;
}

void DX12RenderingBackendNS::updateConstantBufferImpl(const DX12ConstantBuffer& ConstantBuffer, size_t size, const void* ConstantBufferValue)
{
	std::memcpy(ConstantBuffer.mappedMemory, ConstantBufferValue, size);
}

bool DX12RenderingBackendNS::initializeVertexShader(DX12ShaderProgramComponent* rhs, const std::wstring& VSShaderPath)
{
	// Compile the shader code.
	auto l_shaderBuffer = loadShaderBuffer(ShaderType::VERTEX, VSShaderPath);

	rhs->m_vertexShader = l_shaderBuffer;

	return true;
}

bool DX12RenderingBackendNS::initializePixelShader(DX12ShaderProgramComponent* rhs, const std::wstring& PSShaderPath)
{
	// Compile the shader code.
	auto l_shaderBuffer = loadShaderBuffer(ShaderType::FRAGMENT, PSShaderPath);

	rhs->m_pixelShader = l_shaderBuffer;

	return true;
}

bool DX12RenderingBackendNS::createSampler(DX12ShaderProgramComponent* rhs)
{
	rhs->m_samplerCPUHandle = DX12RenderingBackendComponent::get().m_currentSamplerCPUHandle;
	rhs->m_samplerGPUHandle = DX12RenderingBackendComponent::get().m_currentSamplerGPUHandle;

	DX12RenderingBackendComponent::get().m_device->CreateSampler(&rhs->m_samplerDesc, rhs->m_samplerCPUHandle);

	auto l_samplerDescSize = DX12RenderingBackendComponent::get().m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	DX12RenderingBackendComponent::get().m_currentSamplerCPUHandle.ptr += l_samplerDescSize;
	DX12RenderingBackendComponent::get().m_currentSamplerGPUHandle.ptr += l_samplerDescSize;

	return true;
}

DX12ShaderProgramComponent* DX12RenderingBackendNS::addDX12ShaderProgramComponent(EntityID rhs)
{
	auto l_rawPtr = g_pModuleManager->getMemorySystem()->spawnObject(m_DX12ShaderProgramComponentPool, sizeof(DX12ShaderProgramComponent));
	auto l_DXSPC = new(l_rawPtr)DX12ShaderProgramComponent();
	l_DXSPC->m_ParentEntity = rhs;
	return l_DXSPC;
}

bool DX12RenderingBackendNS::initializeDX12ShaderProgramComponent(DX12ShaderProgramComponent* rhs, const ShaderFilePaths& shaderFilePaths)
{
	bool l_result = true;
	if (shaderFilePaths.m_VSPath != "")
	{
		l_result &= initializeVertexShader(rhs, std::wstring(shaderFilePaths.m_VSPath.begin(), shaderFilePaths.m_VSPath.end()));
	}
	if (shaderFilePaths.m_FSPath != "")
	{
		l_result &= initializePixelShader(rhs, std::wstring(shaderFilePaths.m_FSPath.begin(), shaderFilePaths.m_FSPath.end()));
	}

	l_result &= createSampler(rhs);

	return l_result;
}

ID3D12GraphicsCommandList* DX12RenderingBackendNS::beginSingleTimeCommands()
{
	ID3D12GraphicsCommandList* l_commandList;

	// Create a basic command list.
	auto l_result = DX12RenderingBackendComponent::get().m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, DX12RenderingBackendComponent::get().m_globalCommandAllocator, NULL, IID_PPV_ARGS(&l_commandList));
	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: Can't create command list!");
		return nullptr;
	}

	return l_commandList;
}

void DX12RenderingBackendNS::endSingleTimeCommands(ID3D12GraphicsCommandList* commandList)
{
	auto l_result = commandList->Close();
	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: Can't close the command list!");
	}

	ID3D12Fence1* l_uploadFinishFence;
	l_result = DX12RenderingBackendComponent::get().m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&l_uploadFinishFence));
	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: Can't create fence!");
	}

	auto l_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (l_fenceEvent == nullptr)
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: Can't create fence event!");
	}

	ID3D12CommandList* ppCommandLists[] = { commandList };
	DX12RenderingBackendComponent::get().m_globalCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	DX12RenderingBackendComponent::get().m_globalCommandQueue->Signal(l_uploadFinishFence, 1);
	l_uploadFinishFence->SetEventOnCompletion(1, l_fenceEvent);
	WaitForSingleObject(l_fenceEvent, INFINITE);
	CloseHandle(l_fenceEvent);

	commandList->Release();
}

ID3D12Resource* DX12RenderingBackendNS::createUploadHeapBuffer(UINT64 size)
{
	ID3D12Resource* l_uploadHeapBuffer;

	auto l_result = DX12RenderingBackendComponent::get().m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&l_uploadHeapBuffer));

	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: can't create upload heap buffer!");
		return nullptr;
	}

	return l_uploadHeapBuffer;
}

ID3D12Resource* DX12RenderingBackendNS::createDefaultHeapBuffer(D3D12_RESOURCE_DESC* resourceDesc, D3D12_CLEAR_VALUE* clearValue)
{
	ID3D12Resource* l_defaultHeapBuffer;

	auto l_result = DX12RenderingBackendComponent::get().m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		clearValue,
		IID_PPV_ARGS(&l_defaultHeapBuffer));

	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: can't create default heap buffer!");
		return false;
	}

	return l_defaultHeapBuffer;
}

void DX12RenderingBackendNS::OutputShaderErrorMessage(ID3DBlob * errorMessage, HWND hwnd, const std::string & shaderFilename)
{
	char* compileErrors;
	uint64_t bufferSize, i;
	std::stringstream errorSStream;

	// Get a pointer to the error message text buffer.
	compileErrors = (char*)(errorMessage->GetBufferPointer());

	// Get the length of the message.
	bufferSize = errorMessage->GetBufferSize();

	// Write out the error message.
	for (i = 0; i < bufferSize; i++)
	{
		errorSStream << compileErrors[i];
	}

	// Release the error message.
	errorMessage->Release();
	errorMessage = 0;

	MessageBox(WinWindowSystemComponent::get().m_hwnd, errorSStream.str().c_str(), shaderFilename.c_str(), MB_OK);
	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: innoShader: " + shaderFilename + " compile error: " + errorSStream.str() + "\n -- --------------------------------------------------- -- ");
}

DX12RenderPassComponent* DX12RenderingBackendNS::addDX12RenderPassComponent(const EntityID& parentEntity, const char* name)
{
	auto l_rawPtr = g_pModuleManager->getMemorySystem()->spawnObject(m_DX12RenderPassComponentPool, sizeof(DX12RenderPassComponent));
	auto l_DXRPC = new(l_rawPtr)DX12RenderPassComponent();
	l_DXRPC->m_ParentEntity = parentEntity;
	l_DXRPC->m_name = name;
	return l_DXRPC;
}

bool DX12RenderingBackendNS::initializeDX12RenderPassComponent(DX12RenderPassComponent* DXRPC, DX12ShaderProgramComponent* DXSPC)
{
	bool l_result = true;

	l_result &= reserveRenderTargets(DXRPC);

	l_result &= createRenderTargets(DXRPC);

	l_result &= createRTVDescriptorHeap(DXRPC);
	l_result &= createRTV(DXRPC);

	if (DXRPC->m_renderPassDesc.useDepthAttachment)
	{
		l_result &= createDSVDescriptorHeap(DXRPC);
		l_result &= createDSV(DXRPC);
	}

	l_result &= createRootSignature(DXRPC);
	l_result &= createPSO(DXRPC, DXSPC);
	l_result &= createCommandQueue(DXRPC);
	l_result &= createCommandAllocators(DXRPC);
	l_result &= createCommandLists(DXRPC);
	l_result &= createSyncPrimitives(DXRPC);

	DXRPC->m_ObjectStatus = ObjectStatus::Activated;

	return l_result;
}

bool DX12RenderingBackendNS::reserveRenderTargets(DX12RenderPassComponent* DXRPC)
{
	DXRPC->m_RTVDescriptorCPUHandles.reserve(DXRPC->m_renderPassDesc.RTNumber);
	for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		DXRPC->m_RTVDescriptorCPUHandles.emplace_back();
	}

	DXRPC->m_renderTargets.reserve(DXRPC->m_renderPassDesc.RTNumber);
	DXRPC->m_SRVs.reserve(DXRPC->m_renderPassDesc.RTNumber);
	for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		DXRPC->m_renderTargets.emplace_back();
		DXRPC->m_SRVs.emplace_back();
	}

	for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		DXRPC->m_renderTargets[i] = addDX12TextureDataComponent();
	}

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " render targets have been allocated.");

	return true;
}

bool DX12RenderingBackendNS::createRenderTargets(DX12RenderPassComponent* DXRPC)
{
	for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		auto l_TDC = DXRPC->m_renderTargets[i];

		l_TDC->m_textureDataDesc = DXRPC->m_renderPassDesc.RTDesc;

		l_TDC->m_textureData = nullptr;

		initializeDX12TextureDataComponent(l_TDC);

		std::string l_name = DXRPC->m_name.c_str();
		l_name = l_name + "_RT_" + std::to_string(i);
		l_TDC->m_texture->SetName(std::wstring(l_name.begin(), l_name.end()).c_str());

		DXRPC->m_SRVs[i] = createSRV(*DXRPC->m_renderTargets[i]);
	}

	if (DXRPC->m_renderPassDesc.useDepthAttachment)
	{
		DXRPC->m_depthStencilTarget = addDX12TextureDataComponent();
		DXRPC->m_depthStencilTarget->m_textureDataDesc = DXRPC->m_renderPassDesc.RTDesc;

		if (DXRPC->m_renderPassDesc.useStencilAttachment)
		{
			DXRPC->m_depthStencilTarget->m_textureDataDesc.usageType = TextureUsageType::DEPTH_STENCIL_ATTACHMENT;
		}
		else
		{
			DXRPC->m_depthStencilTarget->m_textureDataDesc.usageType = TextureUsageType::DEPTH_ATTACHMENT;
		}

		DXRPC->m_depthStencilTarget->m_textureData = { nullptr };

		initializeDX12TextureDataComponent(DXRPC->m_depthStencilTarget);

		std::string l_name = DXRPC->m_name.c_str();
		l_name = l_name + "_DS";
		DXRPC->m_depthStencilTarget->m_texture->SetName(std::wstring(l_name.begin(), l_name.end()).c_str());
	}

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " render targets have been created.");

	return true;
}

bool DX12RenderingBackendNS::createRTVDescriptorHeap(DX12RenderPassComponent* DXRPC)
{
	DXRPC->m_RTVDescriptorHeapDesc.NumDescriptors = DXRPC->m_renderPassDesc.RTNumber;
	DXRPC->m_RTVDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	DXRPC->m_RTVDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (DXRPC->m_RTVDescriptorHeapDesc.NumDescriptors)
	{
		auto l_result = DX12RenderingBackendComponent::get().m_device->CreateDescriptorHeap(&DXRPC->m_RTVDescriptorHeapDesc, IID_PPV_ARGS(&DXRPC->m_RTVDescriptorHeap));
		if (FAILED(l_result))
		{
			g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " can't create DescriptorHeap for RTV!");
			return false;
		}
		DXRPC->m_RTVDescriptorCPUHandles[0] = DXRPC->m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	}

	std::string l_name = DXRPC->m_name.c_str();
	l_name = l_name + "_RTVDescHeap";
	DXRPC->m_RTVDescriptorHeap->SetName(std::wstring(l_name.begin(), l_name.end()).c_str());

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " RTV DescriptorHeap has been created.");

	return true;
}

bool DX12RenderingBackendNS::createRTV(DX12RenderPassComponent* DXRPC)
{
	auto l_RTVDescSize = DX12RenderingBackendComponent::get().m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	DXRPC->m_RTVDesc.Format = DXRPC->m_renderTargets[0]->m_DX12TextureDataDesc.Format;
	DXRPC->m_RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	DXRPC->m_RTVDesc.Texture2D.MipSlice = 0;

	for (size_t i = 1; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		DXRPC->m_RTVDescriptorCPUHandles[i].ptr = DXRPC->m_RTVDescriptorCPUHandles[i - 1].ptr + l_RTVDescSize;
	}

	for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		DX12RenderingBackendComponent::get().m_device->CreateRenderTargetView(DXRPC->m_renderTargets[i]->m_texture, &DXRPC->m_RTVDesc, DXRPC->m_RTVDescriptorCPUHandles[i]);
	}

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " RTV has been created.");

	return true;
}

bool DX12RenderingBackendNS::createDSVDescriptorHeap(DX12RenderPassComponent* DXRPC)
{
	DXRPC->m_DSVDescriptorHeapDesc.NumDescriptors = 1;
	DXRPC->m_DSVDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	DXRPC->m_DSVDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	auto l_result = DX12RenderingBackendComponent::get().m_device->CreateDescriptorHeap(&DXRPC->m_DSVDescriptorHeapDesc, IID_PPV_ARGS(&DXRPC->m_DSVDescriptorHeap));

	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " can't create DescriptorHeap for DSV!");
		return false;
	}

	DXRPC->m_DSVDescriptorCPUHandle = DXRPC->m_DSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	std::string l_name = DXRPC->m_name.c_str();
	l_name = l_name + "_DSVDescHeap";
	DXRPC->m_DSVDescriptorHeap->SetName(std::wstring(l_name.begin(), l_name.end()).c_str());

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " DSV DescriptorHeap has been created.");

	return true;
}

bool DX12RenderingBackendNS::createDSV(DX12RenderPassComponent* DXRPC)
{
	auto l_DSVDescSize = DX12RenderingBackendComponent::get().m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	if (DXRPC->m_renderPassDesc.useStencilAttachment)
	{
		DXRPC->m_DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	}
	else
	{
		DXRPC->m_DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
	}

	DXRPC->m_DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DXRPC->m_DSVDesc.Texture2D.MipSlice = 0;

	DX12RenderingBackendComponent::get().m_device->CreateDepthStencilView(DXRPC->m_depthStencilTarget->m_texture, &DXRPC->m_DSVDesc, DXRPC->m_DSVDescriptorCPUHandle);

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " DSV has been created.");

	return true;
}

bool DX12RenderingBackendNS::createRootSignature(DX12RenderPassComponent* DXRPC)
{
	ID3DBlob* l_signature;
	ID3DBlob* l_error;

	auto l_result = D3D12SerializeVersionedRootSignature(&DXRPC->m_rootSignatureDesc, &l_signature, &l_error);

	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " can't serialize RootSignature!");
		return false;
	}

	l_result = DX12RenderingBackendComponent::get().m_device->CreateRootSignature(0, l_signature->GetBufferPointer(), l_signature->GetBufferSize(), IID_PPV_ARGS(&DXRPC->m_rootSignature));

	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " can't create Root Signature!");
		return false;
	}

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " RootSignature has been created.");

	return true;
}

bool DX12RenderingBackendNS::createPSO(DX12RenderPassComponent* DXRPC, DX12ShaderProgramComponent* DXSPC)
{
	D3D12_INPUT_ELEMENT_DESC l_polygonLayout[5];
	uint32_t l_numElements;

	// Create the vertex input layout description.
	l_polygonLayout[0].SemanticName = "POSITION";
	l_polygonLayout[0].SemanticIndex = 0;
	l_polygonLayout[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	l_polygonLayout[0].InputSlot = 0;
	l_polygonLayout[0].AlignedByteOffset = 0;
	l_polygonLayout[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	l_polygonLayout[0].InstanceDataStepRate = 0;

	l_polygonLayout[1].SemanticName = "TEXCOORD";
	l_polygonLayout[1].SemanticIndex = 0;
	l_polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	l_polygonLayout[1].InputSlot = 0;
	l_polygonLayout[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	l_polygonLayout[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	l_polygonLayout[1].InstanceDataStepRate = 0;

	l_polygonLayout[2].SemanticName = "PADA";
	l_polygonLayout[2].SemanticIndex = 0;
	l_polygonLayout[2].Format = DXGI_FORMAT_R32G32_FLOAT;
	l_polygonLayout[2].InputSlot = 0;
	l_polygonLayout[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	l_polygonLayout[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	l_polygonLayout[2].InstanceDataStepRate = 0;

	l_polygonLayout[3].SemanticName = "NORMAL";
	l_polygonLayout[3].SemanticIndex = 0;
	l_polygonLayout[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	l_polygonLayout[3].InputSlot = 0;
	l_polygonLayout[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	l_polygonLayout[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	l_polygonLayout[3].InstanceDataStepRate = 0;

	l_polygonLayout[4].SemanticName = "PADB";
	l_polygonLayout[4].SemanticIndex = 0;
	l_polygonLayout[4].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	l_polygonLayout[4].InputSlot = 0;
	l_polygonLayout[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	l_polygonLayout[4].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	l_polygonLayout[4].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	l_numElements = sizeof(l_polygonLayout) / sizeof(l_polygonLayout[0]);

	D3D12_SHADER_BYTECODE l_vsBytecode;
	l_vsBytecode.pShaderBytecode = DXSPC->m_vertexShader->GetBufferPointer();
	l_vsBytecode.BytecodeLength = DXSPC->m_vertexShader->GetBufferSize();

	D3D12_SHADER_BYTECODE l_psBytecode;
	l_psBytecode.pShaderBytecode = DXSPC->m_pixelShader->GetBufferPointer();
	l_psBytecode.BytecodeLength = DXSPC->m_pixelShader->GetBufferSize();

	DXRPC->m_PSODesc.pRootSignature = DXRPC->m_rootSignature;
	DXRPC->m_PSODesc.InputLayout = { l_polygonLayout, l_numElements };
	DXRPC->m_PSODesc.VS = l_vsBytecode;
	DXRPC->m_PSODesc.PS = l_psBytecode;

	DXRPC->m_PSODesc.NumRenderTargets = DXRPC->m_renderPassDesc.RTNumber;
	for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		DXRPC->m_PSODesc.RTVFormats[i] = DXRPC->m_renderTargets[i]->m_DX12TextureDataDesc.Format;
	}

	DXRPC->m_PSODesc.DSVFormat = DXRPC->m_DSVDesc.Format;
	DXRPC->m_PSODesc.DepthStencilState = DXRPC->m_depthStencilDesc;
	DXRPC->m_PSODesc.RasterizerState = DXRPC->m_rasterizerDesc;
	DXRPC->m_PSODesc.BlendState = DXRPC->m_blendDesc;

	auto l_result = DX12RenderingBackendComponent::get().m_device->CreateGraphicsPipelineState(&DXRPC->m_PSODesc, IID_PPV_ARGS(&DXRPC->m_PSO));

	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " can't create PSO!");
		return false;
	}

	std::string l_name = DXRPC->m_name.c_str();
	l_name = l_name + "_PSO";
	DXRPC->m_PSO->SetName(std::wstring(l_name.begin(), l_name.end()).c_str());

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " PSO has been created.");

	return true;
}

bool DX12RenderingBackendNS::createCommandQueue(DX12RenderPassComponent* DXRPC)
{
	// Set up the description of the command queue.
	DXRPC->m_commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DXRPC->m_commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	DXRPC->m_commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	DXRPC->m_commandQueueDesc.NodeMask = 0;

	// Create the command queue.
	auto l_result = DX12RenderingBackendComponent::get().m_device->CreateCommandQueue(&DXRPC->m_commandQueueDesc, IID_PPV_ARGS(&DXRPC->m_commandQueue));
	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " can't create CommandQueue!");
		return false;
	}

	std::string l_name = DXRPC->m_name.c_str();
	l_name = l_name + "_CommandQueue";
	DXRPC->m_commandQueue->SetName(std::wstring(l_name.begin(), l_name.end()).c_str());

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " CommandQueue has been created.");

	return true;
}

bool DX12RenderingBackendNS::createCommandAllocators(DX12RenderPassComponent* DXRPC)
{
	if (DXRPC->m_renderPassDesc.useMultipleFramebuffers)
	{
		DXRPC->m_commandAllocators.resize(DXRPC->m_renderPassDesc.RTNumber);
	}
	else
	{
		DXRPC->m_commandAllocators.resize(1);
	}

	HRESULT l_result;

	for (size_t i = 0; i < DXRPC->m_commandAllocators.size(); i++)
	{
		// Create a command allocator.
		l_result = DX12RenderingBackendComponent::get().m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&DXRPC->m_commandAllocators[i]));
		if (FAILED(l_result))
		{
			g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " can't create CommandAllocator!");
			return false;
		}

		std::string l_name = DXRPC->m_name.c_str();
		l_name = l_name + "_CommandAllocator_" + std::to_string(i);
		DXRPC->m_commandAllocators[i]->SetName(std::wstring(l_name.begin(), l_name.end()).c_str());

		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " CommandAllocator has been created.");
	}

	return true;
}

bool DX12RenderingBackendNS::createCommandLists(DX12RenderPassComponent* DXRPC)
{
	DXRPC->m_commandLists.resize(DXRPC->m_commandAllocators.size());

	for (size_t i = 0; i < DXRPC->m_commandAllocators.size(); i++)
	{
		auto l_result = DX12RenderingBackendComponent::get().m_device->CreateCommandList
		(0, D3D12_COMMAND_LIST_TYPE_DIRECT, DXRPC->m_commandAllocators[i], NULL, IID_PPV_ARGS(&DXRPC->m_commandLists[i]));
		if (FAILED(l_result))
		{
			g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " can't create CommandList!");
			return false;
		}

		DXRPC->m_commandLists[i]->Close();

		std::string l_name = DXRPC->m_name.c_str();
		l_name = l_name + "_CommandList_" + std::to_string(i);
		DXRPC->m_commandLists[i]->SetName(std::wstring(l_name.begin(), l_name.end()).c_str());
	}

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " CommandList has been created.");

	return true;
}

bool DX12RenderingBackendNS::createSyncPrimitives(DX12RenderPassComponent* DXRPC)
{
	HRESULT l_result;

	DXRPC->m_fenceStatus.reserve(DXRPC->m_commandLists.size());

	for (size_t i = 0; i < DXRPC->m_commandLists.size(); i++)
	{
		DXRPC->m_fenceStatus.emplace_back();
	}

	// Create a fence for GPU synchronization.
	l_result = DX12RenderingBackendComponent::get().m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&DXRPC->m_fence));
	if (FAILED(l_result))
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " can't create Fence!");
		return false;
	}

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " Fence has been created.");

	// Create an event object for the fence.
	DXRPC->m_fenceEvent = CreateEventEx(NULL, FALSE, FALSE, EVENT_ALL_ACCESS);
	if (DXRPC->m_fenceEvent == NULL)
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " can't create fence event!");
		return false;
	}

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingBackend: " + std::string(DXRPC->m_name.c_str()) + " Fence event has been created.");

	return true;
}

bool DX12RenderingBackendNS::destroyDX12RenderPassComponent(DX12RenderPassComponent* DXRPC)
{
	DXRPC->m_fence->Release();

	for (auto i : DXRPC->m_commandLists)
	{
		i->Release();
	}

	for (auto i : DXRPC->m_commandAllocators)
	{
		i->Release();
	}

	DXRPC->m_commandQueue->Release();

	DXRPC->m_PSO->Release();

	DXRPC->m_rootSignature->Release();

	if (DXRPC->m_renderPassDesc.useDepthAttachment)
	{
		DXRPC->m_depthStencilTarget->m_texture->Release();
		DXRPC->m_DSVDescriptorHeap->Release();
	}

	for (auto i : DXRPC->m_renderTargets)
	{
		i->m_texture->Release();
	}

	DXRPC->m_RTVDescriptorHeap->Release();

	return true;
}

bool DX12RenderingBackendNS::initializeDX12MeshDataComponent(DX12MeshDataComponent* rhs)
{
	if (rhs->m_ObjectStatus == ObjectStatus::Activated)
	{
		return true;
	}
	else
	{
		submitGPUData(rhs);

		return true;
	}
}

bool DX12RenderingBackendNS::initializeDX12TextureDataComponent(DX12TextureDataComponent * rhs)
{
	if (rhs->m_ObjectStatus == ObjectStatus::Activated)
	{
		return true;
	}
	else
	{
		if (rhs->m_textureDataDesc.usageType == TextureUsageType::INVISIBLE)
		{
			g_pModuleManager->getLogSystem()->printLog(LogType::INNO_WARNING, "DX12RenderingBackend: try to generate DX12TextureDataComponent for TextureUsageType::INVISIBLE type!");
			return false;
		}
		else if (rhs->m_textureDataDesc.usageType == TextureUsageType::COLOR_ATTACHMENT
			|| rhs->m_textureDataDesc.usageType == TextureUsageType::DEPTH_ATTACHMENT
			|| rhs->m_textureDataDesc.usageType == TextureUsageType::DEPTH_STENCIL_ATTACHMENT
			|| rhs->m_textureDataDesc.usageType == TextureUsageType::RAW_IMAGE)
		{
			submitGPUData(rhs);
			return true;
		}
		else
		{
			if (rhs->m_textureData)
			{
				submitGPUData(rhs);

				if (rhs->m_textureDataDesc.usageType != TextureUsageType::COLOR_ATTACHMENT)
				{
					// @TODO: release raw data in heap memory
				}

				return true;
			}
			else
			{
				g_pModuleManager->getLogSystem()->printLog(LogType::INNO_WARNING, "DX12RenderingBackend: try to generate DX12TextureDataComponent without raw data!");
				return false;
			}
		}
	}
}

DX12SRV DX12RenderingBackendNS::createSRV(const DX12TextureDataComponent & rhs)
{
	DX12SRV l_result;

	uint32_t l_mipLevels = -1;
	if (rhs.m_textureDataDesc.usageType == TextureUsageType::COLOR_ATTACHMENT
		|| rhs.m_textureDataDesc.usageType == TextureUsageType::DEPTH_ATTACHMENT
		|| rhs.m_textureDataDesc.usageType == TextureUsageType::DEPTH_STENCIL_ATTACHMENT
		|| rhs.m_textureDataDesc.usageType == TextureUsageType::RAW_IMAGE)
	{
		l_mipLevels = 1;
	}

	l_result.SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	l_result.SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	l_result.SRVDesc.Texture2D.MostDetailedMip = 0;
	l_result.SRVDesc.Texture2D.MipLevels = l_mipLevels;

	if (rhs.m_textureDataDesc.usageType == TextureUsageType::DEPTH_ATTACHMENT)
	{
		l_result.SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	}
	else if (rhs.m_textureDataDesc.usageType == TextureUsageType::DEPTH_STENCIL_ATTACHMENT)
	{
		l_result.SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	}
	else
	{
		l_result.SRVDesc.Format = rhs.m_DX12TextureDataDesc.Format;
	}

	l_result.CPUHandle = DX12RenderingBackendComponent::get().m_currentCSUCPUHandle;
	l_result.GPUHandle = DX12RenderingBackendComponent::get().m_currentCSUGPUHandle;

	auto l_CSUDescSize = DX12RenderingBackendComponent::get().m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	DX12RenderingBackendComponent::get().m_currentCSUCPUHandle.ptr += l_CSUDescSize;
	DX12RenderingBackendComponent::get().m_currentCSUGPUHandle.ptr += l_CSUDescSize;

	DX12RenderingBackendComponent::get().m_device->CreateShaderResourceView(rhs.m_texture, &l_result.SRVDesc, l_result.CPUHandle);

	return l_result;
}

bool DX12RenderingBackendNS::initializeDX12MaterialDataComponent(DX12MaterialDataComponent * rhs)
{
	if (rhs->m_ObjectStatus == ObjectStatus::Activated)
	{
		return true;
	}
	else
	{
		submitGPUData(rhs);

		return true;
	}
}

bool DX12RenderingBackendNS::destroyAllGraphicPrimitiveComponents()
{
	for (auto i : m_initializedDX12Meshes)
	{
		i.second->m_indexBuffer->Release();
		i.second->m_vertexBuffer->Release();
	}
	for (auto i : m_initializedDX12Textures)
	{
		i.second->m_texture->Release();
	}

	return true;
}

bool DX12RenderingBackendNS::submitGPUData(DX12MeshDataComponent * rhs)
{
	// vertices
	auto l_verticesDataSize = uint32_t(sizeof(Vertex) * rhs->m_vertices.size());

	auto l_verticesResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(l_verticesDataSize);
	rhs->m_vertexBuffer = createDefaultHeapBuffer(&l_verticesResourceDesc);

	if (rhs->m_vertexBuffer == nullptr)
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: can't create vertex buffer!");
		return false;
	}

	auto l_uploadHeapBuffer = createUploadHeapBuffer(l_verticesDataSize);

	auto l_commandList = beginSingleTimeCommands();

	// main memory ----> upload heap
	D3D12_SUBRESOURCE_DATA l_verticesSubResourceData = {};
	l_verticesSubResourceData.pData = &rhs->m_vertices[0];
	l_verticesSubResourceData.RowPitch = l_verticesDataSize;
	l_verticesSubResourceData.SlicePitch = 1;
	UpdateSubresources(l_commandList, rhs->m_vertexBuffer, l_uploadHeapBuffer, 0, 0, 1, &l_verticesSubResourceData);

	//  upload heap ----> default heap
	l_commandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(rhs->m_vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// Initialize the vertex buffer view.
	rhs->m_VBV.BufferLocation = rhs->m_vertexBuffer->GetGPUVirtualAddress();
	rhs->m_VBV.StrideInBytes = sizeof(Vertex);
	rhs->m_VBV.SizeInBytes = l_verticesDataSize;

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_VERBOSE, "DX12RenderingBackend: VBO " + InnoUtility::pointerToString(rhs->m_vertexBuffer) + " is initialized.");

	// indices
	auto l_indicesDataSize = uint32_t(sizeof(Index) * rhs->m_indices.size());

	auto l_indicesResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(l_indicesDataSize);
	rhs->m_indexBuffer = createDefaultHeapBuffer(&l_indicesResourceDesc);

	if (rhs->m_indexBuffer == nullptr)
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: can't create index buffer!");
		return false;
	}

	l_uploadHeapBuffer = createUploadHeapBuffer(l_indicesDataSize);

	// main memory ----> upload heap
	D3D12_SUBRESOURCE_DATA l_indicesSubResourceData = {};
	l_indicesSubResourceData.pData = &rhs->m_indices[0];
	l_indicesSubResourceData.RowPitch = l_indicesDataSize;
	l_indicesSubResourceData.SlicePitch = 1;
	UpdateSubresources(l_commandList, rhs->m_indexBuffer, l_uploadHeapBuffer, 0, 0, 1, &l_indicesSubResourceData);

	//  upload heap ----> default heap
	l_commandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(rhs->m_indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

	endSingleTimeCommands(l_commandList);

	// Initialize the index buffer view.
	rhs->m_IBV.Format = DXGI_FORMAT_R32_UINT;
	rhs->m_IBV.BufferLocation = rhs->m_indexBuffer->GetGPUVirtualAddress();
	rhs->m_IBV.SizeInBytes = l_indicesDataSize;

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_VERBOSE, "DX12RenderingBackend: IBO " + InnoUtility::pointerToString(rhs->m_indexBuffer) + " is initialized.");

	rhs->m_ObjectStatus = ObjectStatus::Activated;

	m_initializedDX12Meshes.emplace(rhs->m_ParentEntity, rhs);

	return true;
}

DXGI_FORMAT DX12RenderingBackendNS::getTextureFormat(TextureDataDesc textureDataDesc)
{
	DXGI_FORMAT l_internalFormat = DXGI_FORMAT_UNKNOWN;

	if (textureDataDesc.usageType == TextureUsageType::ALBEDO)
	{
		l_internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	}
	else if (textureDataDesc.usageType == TextureUsageType::DEPTH_ATTACHMENT)
	{
		l_internalFormat = DXGI_FORMAT_D32_FLOAT;
	}
	else if (textureDataDesc.usageType == TextureUsageType::DEPTH_STENCIL_ATTACHMENT)
	{
		l_internalFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	}
	else
	{
		if (textureDataDesc.pixelDataType == TexturePixelDataType::UBYTE)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R8_UNORM; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R8G8_UNORM; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
			default: break;
			}
		}
		else if (textureDataDesc.pixelDataType == TexturePixelDataType::SBYTE)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R8_SNORM; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R8G8_SNORM; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R8G8B8A8_SNORM; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R8G8B8A8_SNORM; break;
			default: break;
			}
		}
		else if (textureDataDesc.pixelDataType == TexturePixelDataType::USHORT)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R16_UNORM; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R16G16_UNORM; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R16G16B16A16_UNORM; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R16G16B16A16_UNORM; break;
			default: break;
			}
		}
		else if (textureDataDesc.pixelDataType == TexturePixelDataType::SSHORT)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R16_SNORM; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R16G16_SNORM; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R16G16B16A16_SNORM; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R16G16B16A16_SNORM; break;
			default: break;
			}
		}
		if (textureDataDesc.pixelDataType == TexturePixelDataType::UINT8)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R8_UINT; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R8G8_UINT; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UINT; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UINT; break;
			default: break;
			}
		}
		else if (textureDataDesc.pixelDataType == TexturePixelDataType::SINT8)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R8_SINT; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R8G8_SINT; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R8G8B8A8_SINT; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R8G8B8A8_SINT; break;
			default: break;
			}
		}
		else if (textureDataDesc.pixelDataType == TexturePixelDataType::UINT16)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R16_UINT; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R16G16_UINT; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R16G16B16A16_UINT; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R16G16B16A16_UINT; break;
			default: break;
			}
		}
		else if (textureDataDesc.pixelDataType == TexturePixelDataType::SINT16)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R16_SINT; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R16G16_SINT; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R16G16B16A16_SINT; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R16G16B16A16_SINT; break;
			default: break;
			}
		}
		else if (textureDataDesc.pixelDataType == TexturePixelDataType::UINT32)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R32_UINT; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R32G32_UINT; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R32G32B32A32_UINT; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R32G32B32A32_UINT; break;
			default: break;
			}
		}
		else if (textureDataDesc.pixelDataType == TexturePixelDataType::SINT32)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R32_SINT; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R32G32_SINT; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R32G32B32A32_SINT; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R32G32B32A32_SINT; break;
			default: break;
			}
		}
		else if (textureDataDesc.pixelDataType == TexturePixelDataType::FLOAT16)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R16_FLOAT; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R16G16_FLOAT; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
			default: break;
			}
		}
		else if (textureDataDesc.pixelDataType == TexturePixelDataType::FLOAT32)
		{
			switch (textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::R: l_internalFormat = DXGI_FORMAT_R32_FLOAT; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R32G32_FLOAT; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
			default: break;
			}
		}
	}

	return l_internalFormat;
}

uint32_t DX12RenderingBackendNS::getTextureMipLevels(TextureDataDesc textureDataDesc)
{
	uint32_t textureMipLevels = 1;
	if (textureDataDesc.magFilterMethod == TextureFilterMethod::LINEAR_MIPMAP_LINEAR)
	{
		textureMipLevels = 0;
	}

	return textureMipLevels;
}

D3D12_RESOURCE_FLAGS DX12RenderingBackendNS::getTextureBindFlags(TextureDataDesc textureDataDesc)
{
	D3D12_RESOURCE_FLAGS textureBindFlags = {};
	if (textureDataDesc.usageType == TextureUsageType::COLOR_ATTACHMENT)
	{
		textureBindFlags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}
	else if (textureDataDesc.usageType == TextureUsageType::DEPTH_ATTACHMENT)
	{
		textureBindFlags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	else if (textureDataDesc.usageType == TextureUsageType::DEPTH_STENCIL_ATTACHMENT)
	{
		textureBindFlags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	else if (textureDataDesc.usageType == TextureUsageType::RAW_IMAGE)
	{
		textureBindFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	return textureBindFlags;
}

D3D12_RESOURCE_DESC DX12RenderingBackendNS::getDX12TextureDataDesc(TextureDataDesc textureDataDesc)
{
	D3D12_RESOURCE_DESC DX12TextureDataDesc = {};

	DX12TextureDataDesc.Height = textureDataDesc.height;
	DX12TextureDataDesc.Width = textureDataDesc.width;
	DX12TextureDataDesc.MipLevels = getTextureMipLevels(textureDataDesc);
	DX12TextureDataDesc.DepthOrArraySize = 1;
	DX12TextureDataDesc.Format = getTextureFormat(textureDataDesc);
	DX12TextureDataDesc.SampleDesc.Count = 1;
	DX12TextureDataDesc.SampleDesc.Quality = 0;
	DX12TextureDataDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DX12TextureDataDesc.Flags = getTextureBindFlags(textureDataDesc);

	return DX12TextureDataDesc;
}

bool DX12RenderingBackendNS::submitGPUData(DX12TextureDataComponent * rhs)
{
	rhs->m_DX12TextureDataDesc = getDX12TextureDataDesc(rhs->m_textureDataDesc);

	// Create the empty texture.
	if (rhs->m_textureDataDesc.usageType == TextureUsageType::COLOR_ATTACHMENT
		|| rhs->m_textureDataDesc.usageType == TextureUsageType::DEPTH_ATTACHMENT
		|| rhs->m_textureDataDesc.usageType == TextureUsageType::DEPTH_STENCIL_ATTACHMENT
		|| rhs->m_textureDataDesc.usageType == TextureUsageType::RAW_IMAGE)
	{
		D3D12_CLEAR_VALUE l_clearValue;
		if (rhs->m_textureDataDesc.usageType == TextureUsageType::COLOR_ATTACHMENT)
		{
			l_clearValue = D3D12_CLEAR_VALUE{ rhs->m_DX12TextureDataDesc.Format, { 0.0f, 0.0f, 0.0f, 1.0f } };
		}
		else if (rhs->m_textureDataDesc.usageType == TextureUsageType::DEPTH_ATTACHMENT)
		{
			l_clearValue.Format = DXGI_FORMAT_D32_FLOAT;
			l_clearValue.DepthStencil = D3D12_DEPTH_STENCIL_VALUE{ 1.0f, 0x00 };
		}
		else if (rhs->m_textureDataDesc.usageType == TextureUsageType::DEPTH_STENCIL_ATTACHMENT)
		{
			l_clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			l_clearValue.DepthStencil = D3D12_DEPTH_STENCIL_VALUE{ 1.0f, 0x00 };
		}
		rhs->m_texture = createDefaultHeapBuffer(&rhs->m_DX12TextureDataDesc, &l_clearValue);
	}
	else
	{
		rhs->m_texture = createDefaultHeapBuffer(&rhs->m_DX12TextureDataDesc);
	}

	if (rhs->m_texture == nullptr)
	{
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingBackend: can't create texture!");
		return false;
	}

	auto l_commandList = beginSingleTimeCommands();

	// main memory ----> upload heap
	if (!(rhs->m_textureDataDesc.usageType == TextureUsageType::COLOR_ATTACHMENT
		|| rhs->m_textureDataDesc.usageType == TextureUsageType::DEPTH_ATTACHMENT
		|| rhs->m_textureDataDesc.usageType == TextureUsageType::DEPTH_STENCIL_ATTACHMENT
		|| rhs->m_textureDataDesc.usageType == TextureUsageType::RAW_IMAGE))
	{
		const UINT64 l_uploadHeapBufferSize = GetRequiredIntermediateSize(rhs->m_texture, 0, 1);

		auto l_uploadHeapBuffer = createUploadHeapBuffer(l_uploadHeapBufferSize);

		D3D12_SUBRESOURCE_DATA l_textureSubResourceData = {};
		l_textureSubResourceData.pData = rhs->m_textureData;
		l_textureSubResourceData.RowPitch = rhs->m_textureDataDesc.width * ((uint32_t)rhs->m_textureDataDesc.pixelDataFormat + 1);
		l_textureSubResourceData.SlicePitch = l_textureSubResourceData.RowPitch * rhs->m_textureDataDesc.height;
		UpdateSubresources(l_commandList, rhs->m_texture, l_uploadHeapBuffer, 0, 0, 1, &l_textureSubResourceData);
	}

	//  upload heap ----> default heap
	if (rhs->m_textureDataDesc.usageType == TextureUsageType::COLOR_ATTACHMENT)
	{
		l_commandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(rhs->m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET));
	}
	else if (rhs->m_textureDataDesc.usageType == TextureUsageType::DEPTH_ATTACHMENT || rhs->m_textureDataDesc.usageType == TextureUsageType::DEPTH_STENCIL_ATTACHMENT)
	{
		l_commandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(rhs->m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	}
	else if (rhs->m_textureDataDesc.usageType == TextureUsageType::RAW_IMAGE)
	{
		l_commandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(rhs->m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}
	else
	{
		l_commandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(rhs->m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}

	endSingleTimeCommands(l_commandList);

	g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_VERBOSE, "DX12RenderingBackend: texture " + InnoUtility::pointerToString(rhs) + " is initialized.");

	rhs->m_ObjectStatus = ObjectStatus::Activated;

	m_initializedDX12Textures.emplace(rhs->m_ParentEntity, rhs);

	return true;
}

bool DX12RenderingBackendNS::submitGPUData(DX12MaterialDataComponent * rhs)
{
	rhs->m_SRVs.resize(5);

	if (rhs->m_normalTexture)
	{
		initializeDX12TextureDataComponent(reinterpret_cast<DX12TextureDataComponent*>(rhs->m_normalTexture));
		rhs->m_SRVs[0] = createSRV(*reinterpret_cast<DX12TextureDataComponent*>(rhs->m_normalTexture));
	}
	else
	{
		rhs->m_SRVs[0] = createSRV(*getDX12TextureDataComponent(TextureUsageType::NORMAL));
	}
	if (rhs->m_albedoTexture)
	{
		initializeDX12TextureDataComponent(reinterpret_cast<DX12TextureDataComponent*>(rhs->m_albedoTexture));
		rhs->m_SRVs[1] = createSRV(*reinterpret_cast<DX12TextureDataComponent*>(rhs->m_albedoTexture));
	}
	else
	{
		rhs->m_SRVs[1] = createSRV(*getDX12TextureDataComponent(TextureUsageType::ALBEDO));
	}
	if (rhs->m_metallicTexture)
	{
		initializeDX12TextureDataComponent(reinterpret_cast<DX12TextureDataComponent*>(rhs->m_metallicTexture));
		rhs->m_SRVs[2] = createSRV(*reinterpret_cast<DX12TextureDataComponent*>(rhs->m_metallicTexture));
	}
	else
	{
		rhs->m_SRVs[2] = createSRV(*getDX12TextureDataComponent(TextureUsageType::METALLIC));
	}
	if (rhs->m_roughnessTexture)
	{
		initializeDX12TextureDataComponent(reinterpret_cast<DX12TextureDataComponent*>(rhs->m_roughnessTexture));
		rhs->m_SRVs[3] = createSRV(*reinterpret_cast<DX12TextureDataComponent*>(rhs->m_roughnessTexture));
	}
	else
	{
		rhs->m_SRVs[3] = createSRV(*getDX12TextureDataComponent(TextureUsageType::ROUGHNESS));
	}
	if (rhs->m_aoTexture)
	{
		initializeDX12TextureDataComponent(reinterpret_cast<DX12TextureDataComponent*>(rhs->m_aoTexture));
		rhs->m_SRVs[4] = createSRV(*reinterpret_cast<DX12TextureDataComponent*>(rhs->m_aoTexture));
	}
	else
	{
		rhs->m_SRVs[4] = createSRV(*getDX12TextureDataComponent(TextureUsageType::AMBIENT_OCCLUSION));
	}

	rhs->m_ObjectStatus = ObjectStatus::Activated;

	m_initializedDX12Materials.emplace(rhs->m_ParentEntity, rhs);

	return true;
}

bool DX12RenderingBackendNS::recordCommandBegin(DX12RenderPassComponent* DXRPC, uint32_t frameIndex)
{
	DXRPC->m_commandAllocators[frameIndex]->Reset();
	DXRPC->m_commandLists[frameIndex]->Reset(DXRPC->m_commandAllocators[frameIndex], DXRPC->m_PSO);

	return true;
}

bool DX12RenderingBackendNS::recordActivateRenderPass(DX12RenderPassComponent* DXRPC, uint32_t frameIndex)
{
	DXRPC->m_commandLists[frameIndex]->SetGraphicsRootSignature(DXRPC->m_rootSignature);
	DXRPC->m_commandLists[frameIndex]->RSSetViewports(1, &DXRPC->m_viewport);
	DXRPC->m_commandLists[frameIndex]->RSSetScissorRects(1, &DXRPC->m_scissor);

	DXRPC->m_commandLists[frameIndex]->SetPipelineState(DXRPC->m_PSO);

	const float l_clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	if (DXRPC->m_renderPassDesc.useMultipleFramebuffers)
	{
		DXRPC->m_commandLists[frameIndex]->OMSetRenderTargets(1, &DXRPC->m_RTVDescriptorCPUHandles[frameIndex], FALSE, &DXRPC->m_DSVDescriptorCPUHandle);
		DXRPC->m_commandLists[frameIndex]->ClearRenderTargetView(DXRPC->m_RTVDescriptorCPUHandles[frameIndex], l_clearColor, 0, nullptr);
	}
	else
	{
		DXRPC->m_commandLists[frameIndex]->OMSetRenderTargets(DXRPC->m_renderPassDesc.RTNumber, &DXRPC->m_RTVDescriptorCPUHandles[0], FALSE, &DXRPC->m_DSVDescriptorCPUHandle);
		for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
		{
			DXRPC->m_commandLists[frameIndex]->ClearRenderTargetView(DXRPC->m_RTVDescriptorCPUHandles[i], l_clearColor, 0, nullptr);
		}
	}

	DXRPC->m_commandLists[frameIndex]->ClearDepthStencilView(DXRPC->m_DSVDescriptorCPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0x00, 0, nullptr);
	return true;
}

bool DX12RenderingBackendNS::recordBindDescHeaps(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, uint32_t heapsCount, ID3D12DescriptorHeap** heaps)
{
	DXRPC->m_commandLists[frameIndex]->SetDescriptorHeaps(heapsCount, heaps);
	return true;
}

bool DX12RenderingBackendNS::recordBindCBV(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, uint32_t startSlot, const DX12ConstantBuffer& ConstantBuffer, size_t offset)
{
	DXRPC->m_commandLists[frameIndex]->SetGraphicsRootConstantBufferView(startSlot, ConstantBuffer.m_constantBuffer->GetGPUVirtualAddress() + offset * ConstantBuffer.elementSize);
	return true;
}

bool DX12RenderingBackendNS::recordBindRTForWrite(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, DX12TextureDataComponent* DXTDC)
{
	DXRPC->m_commandLists[frameIndex]->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(DXTDC->m_texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

	return true;
}

bool DX12RenderingBackendNS::recordBindRTForRead(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, DX12TextureDataComponent* DXTDC)
{
	DXRPC->m_commandLists[frameIndex]->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(DXTDC->m_texture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	return true;
}

bool DX12RenderingBackendNS::recordBindSRVDescTable(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, uint32_t startSlot, const DX12SRV& SRV)
{
	DXRPC->m_commandLists[frameIndex]->SetGraphicsRootDescriptorTable(startSlot, SRV.GPUHandle);

	return true;
}

bool DX12RenderingBackendNS::recordBindSamplerDescTable(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, uint32_t startSlot, DX12ShaderProgramComponent* DXSPC)
{
	DXRPC->m_commandLists[frameIndex]->SetGraphicsRootDescriptorTable(startSlot, DXSPC->m_samplerGPUHandle);

	return true;
}

bool DX12RenderingBackendNS::recordDrawCall(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, DX12MeshDataComponent * DXMDC)
{
	DXRPC->m_commandLists[frameIndex]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DXRPC->m_commandLists[frameIndex]->IASetVertexBuffers(0, 1, &DXMDC->m_VBV);
	DXRPC->m_commandLists[frameIndex]->IASetIndexBuffer(&DXMDC->m_IBV);
	DXRPC->m_commandLists[frameIndex]->DrawIndexedInstanced((uint32_t)DXMDC->m_indicesSize, 1, 0, 0, 0);

	return true;
}

bool DX12RenderingBackendNS::recordCommandEnd(DX12RenderPassComponent* DXRPC, uint32_t frameIndex)
{
	DXRPC->m_commandLists[frameIndex]->Close();

	return true;
}

bool DX12RenderingBackendNS::executeCommandList(DX12RenderPassComponent* DXRPC, uint32_t frameIndex)
{
	ID3D12CommandList* ppCommandLists[] = { DXRPC->m_commandLists[frameIndex] };
	DXRPC->m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	// Schedule a Signal command in the queue.
	const UINT64 finishedFenceValue = DXRPC->m_fenceStatus[frameIndex] + 1;
	DXRPC->m_commandQueue->Signal(DXRPC->m_fence, finishedFenceValue);

	return true;
}

bool DX12RenderingBackendNS::waitFrame(DX12RenderPassComponent* DXRPC, uint32_t frameIndex)
{
	const UINT64 currentFenceValue = DXRPC->m_fence->GetCompletedValue();
	const UINT64 expectedFenceValue = DXRPC->m_fenceStatus[frameIndex] + 1;

	if (currentFenceValue < expectedFenceValue)
	{
		DXRPC->m_fence->SetEventOnCompletion(expectedFenceValue, DXRPC->m_fenceEvent);
		WaitForSingleObjectEx(DXRPC->m_fenceEvent, INFINITE, FALSE);
	}

	DXRPC->m_fenceStatus[frameIndex] = expectedFenceValue;

	return true;
}