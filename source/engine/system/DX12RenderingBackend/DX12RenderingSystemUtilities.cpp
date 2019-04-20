#include "DX12RenderingSystemUtilities.h"

#include "../../component/WinWindowSystemComponent.h"
#include "../../component/DX12RenderingSystemComponent.h"

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE DX12RenderingSystemNS
{
	ID3D12GraphicsCommandList* beginSingleTimeCommands();
	void endSingleTimeCommands(ID3D12GraphicsCommandList* commandList);

	void OutputShaderErrorMessage(ID3DBlob * errorMessage, HWND hwnd, const std::string & shaderFilename);
	ID3DBlob* loadShaderBuffer(ShaderType shaderType, const std::wstring & shaderFilePath);
	bool createCBuffer(DX12CBuffer& arg);
	bool initializeVertexShader(DX12ShaderProgramComponent* rhs, const std::wstring& VSShaderPath);
	bool initializePixelShader(DX12ShaderProgramComponent* rhs, const std::wstring& PSShaderPath);

	bool summitGPUData(DX12MeshDataComponent* rhs);
	bool summitGPUData(DX12TextureDataComponent* rhs);

	std::unordered_map<EntityID, DX12MeshDataComponent*> m_initializedDXMDC;
	std::unordered_map<EntityID, DX12TextureDataComponent*> m_initializedDXTDC;

	std::unordered_map<EntityID, DX12MeshDataComponent*> m_meshMap;
	std::unordered_map<EntityID, DX12TextureDataComponent*> m_textureMap;

	void* m_DX12RenderPassComponentPool;
	void* m_DX12ShaderProgramComponentPool;

	const std::wstring m_shaderRelativePath = L"//res//shaders//";
}

bool DX12RenderingSystemNS::initializeComponentPool()
{
	m_DX12RenderPassComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(DX12RenderPassComponent), 128);
	m_DX12ShaderProgramComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(DX12ShaderProgramComponent), 256);

	return true;
}

ID3DBlob* DX12RenderingSystemNS::loadShaderBuffer(ShaderType shaderType, const std::wstring & shaderFilePath)
{
	auto l_shaderName = std::string(shaderFilePath.begin(), shaderFilePath.end());
	std::reverse(l_shaderName.begin(), l_shaderName.end());
	l_shaderName = l_shaderName.substr(l_shaderName.find(".") + 1, l_shaderName.find("//") - l_shaderName.find(".") - 1);
	std::reverse(l_shaderName.begin(), l_shaderName.end());

	HRESULT result;
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

	auto l_workingDir = g_pCoreSystem->getFileSystem()->getWorkingDirectory();
	auto l_workingDirW = std::wstring(l_workingDir.begin(), l_workingDir.end());

	result = D3DCompileFromFile((l_workingDirW + m_shaderRelativePath + shaderFilePath).c_str(), NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", l_shaderTypeName.c_str(), compileFlags, 0,
		&l_shaderBuffer, &l_errorMessage);
	if (FAILED(result))
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
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "Shader creation failed: cannot find shader!");
		}

		return nullptr;
	}
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_VERBOSE, "DX12RenderingSystem: innoShader: " + l_shaderName + " Shader has been compiled.");
	return l_shaderBuffer;
}

bool DX12RenderingSystemNS::createCBuffer(DX12CBuffer& arg)
{
	return true;
}

bool DX12RenderingSystemNS::initializeVertexShader(DX12ShaderProgramComponent* rhs, const std::wstring& VSShaderPath)
{
	// Compile the shader code.
	auto l_shaderBuffer = loadShaderBuffer(ShaderType::VERTEX, VSShaderPath);

	rhs->m_vertexShader = l_shaderBuffer;

	return true;
}

bool DX12RenderingSystemNS::initializePixelShader(DX12ShaderProgramComponent* rhs, const std::wstring& PSShaderPath)
{
	// Compile the shader code.
	auto l_shaderBuffer = loadShaderBuffer(ShaderType::FRAGMENT, PSShaderPath);

	rhs->m_pixelShader = l_shaderBuffer;

	return true;
}

DX12ShaderProgramComponent* DX12RenderingSystemNS::addDX12ShaderProgramComponent(EntityID rhs)
{
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_DX12ShaderProgramComponentPool, sizeof(DX12ShaderProgramComponent));
	auto l_DXSPC = new(l_rawPtr)DX12ShaderProgramComponent();
	l_DXSPC->m_parentEntity = rhs;
	return l_DXSPC;
}

bool DX12RenderingSystemNS::initializeDX12ShaderProgramComponent(DX12ShaderProgramComponent* rhs, const ShaderFilePaths& shaderFilePaths)
{
	bool l_result = true;
	if (shaderFilePaths.m_VSPath != "")
	{
		l_result = l_result && initializeVertexShader(rhs, std::wstring(shaderFilePaths.m_VSPath.begin(), shaderFilePaths.m_VSPath.end()));
	}
	if (shaderFilePaths.m_FSPath != "")
	{
		l_result = l_result && initializePixelShader(rhs, std::wstring(shaderFilePaths.m_FSPath.begin(), shaderFilePaths.m_FSPath.end()));
	}

	return l_result;
}

ID3D12GraphicsCommandList* DX12RenderingSystemNS::beginSingleTimeCommands()
{
	ID3D12GraphicsCommandList* l_commandList;

	// Create a basic command list.
	auto hResult = DX12RenderingSystemComponent::get().m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, DX12RenderingSystemComponent::get().m_commandAllocator, NULL, IID_PPV_ARGS(&l_commandList));
	if (FAILED(hResult))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: Can't create command list!");
		return nullptr;
	}

	return l_commandList;
}

void DX12RenderingSystemNS::endSingleTimeCommands(ID3D12GraphicsCommandList* commandList)
{
	auto hResult = commandList->Close();
	if (FAILED(hResult))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: Can't close the command list!");
	}

	ID3D12Fence1* l_uploadFinishFence;
	hResult = DX12RenderingSystemComponent::get().m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&l_uploadFinishFence));
	if (FAILED(hResult))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: Can't create fence!");
	}

	auto l_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (l_fenceEvent == nullptr)
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: Can't create fence event!");
	}

	ID3D12CommandList* ppCommandLists[] = { commandList };
	DX12RenderingSystemComponent::get().m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	DX12RenderingSystemComponent::get().m_commandQueue->Signal(l_uploadFinishFence, 1);
	l_uploadFinishFence->SetEventOnCompletion(1, l_fenceEvent);
	WaitForSingleObject(l_fenceEvent, INFINITE);
	CloseHandle(l_fenceEvent);

	commandList->Release();
}

void DX12RenderingSystemNS::OutputShaderErrorMessage(ID3DBlob * errorMessage, HWND hwnd, const std::string & shaderFilename)
{
	char* compileErrors;
	unsigned long long bufferSize, i;
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
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: innoShader: " + shaderFilename + " compile error: " + errorSStream.str() + "\n -- --------------------------------------------------- -- ");
}

bool DX12RenderingSystemNS::activateDX12ShaderProgramComponent(DX12ShaderProgramComponent * rhs)
{
	return true;
}

DX12RenderPassComponent* DX12RenderingSystemNS::addDX12RenderPassComponent(EntityID rhs)
{
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_DX12RenderPassComponentPool, sizeof(DX12RenderPassComponent));
	auto l_DXRPC = new(l_rawPtr)DX12RenderPassComponent();
	l_DXRPC->m_parentEntity = rhs;
	return l_DXRPC;
}

bool DX12RenderingSystemNS::initializeDX12RenderPassComponent(DX12RenderPassComponent* DXRPC, DX12ShaderProgramComponent* DXSPC)
{
	bool result = true;

	result &= reserveRenderTargets(DXRPC);

	result &= createRenderTargets(DXRPC);

	result = createDescriptorHeap(DXRPC);
	result = createRootSignature(DXRPC);
	result = createPSO(DXRPC, DXSPC);

	DXRPC->m_objectStatus = ObjectStatus::ALIVE;

	return DXRPC;
}

bool DX12RenderingSystemNS::reserveRenderTargets(DX12RenderPassComponent* DXRPC)
{
	size_t l_framebufferNumber = 0;
	if (DXRPC->m_renderPassDesc.useMultipleFramebuffers)
	{
		l_framebufferNumber = DXRPC->m_renderPassDesc.RTNumber;
	}
	else
	{
		l_framebufferNumber = 1;
	}

	// reserve vectors and emplace empty objects
	DXRPC->m_RTVs.reserve(DXRPC->m_renderPassDesc.RTNumber);
	for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		DXRPC->m_RTVs.emplace_back();
	}

	DXRPC->m_DXTDCs.reserve(DXRPC->m_renderPassDesc.RTNumber);
	for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		DXRPC->m_DXTDCs.emplace_back();
	}

	for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		DXRPC->m_DXTDCs[i] = addDX12TextureDataComponent();
	}

	return true;
}

bool DX12RenderingSystemNS::createRenderTargets(DX12RenderPassComponent* DXRPC)
{
	for (size_t i = 0; i < DXRPC->m_renderPassDesc.RTNumber; i++)
	{
		auto l_TDC = DXRPC->m_DXTDCs[i];

		l_TDC->m_textureDataDesc = DXRPC->m_renderPassDesc.RTDesc;

		if (l_TDC->m_textureDataDesc.samplerType == TextureSamplerType::CUBEMAP)
		{
			l_TDC->m_textureData = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		}
		else
		{
			l_TDC->m_textureData = { nullptr };
		}

		initializeDX12TextureDataComponent(l_TDC);
	}

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingSystem: Render targets have been created.");

	return true;
}

bool DX12RenderingSystemNS::createDescriptorHeap(DX12RenderPassComponent* DXRPC)
{
	auto l_result = DX12RenderingSystemComponent::get().m_device->CreateDescriptorHeap(&DXRPC->m_RTVHeapDesc, IID_PPV_ARGS(&DXRPC->m_RTVHeap));
	if (FAILED(l_result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: Can't create DescriptorHeap!");
		return false;
	}

	DXRPC->m_RTVDescHandle = DXRPC->m_RTVHeap->GetCPUDescriptorHandleForHeapStart();

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingSystem: DescriptorHeap has been created.");

	return true;
}

bool DX12RenderingSystemNS::createRootSignature(DX12RenderPassComponent* DXRPC)
{
	ID3DBlob* l_signature;
	ID3DBlob* l_error;

	auto l_result = D3D12SerializeVersionedRootSignature(&DXRPC->m_rootSignatureDesc, &l_signature, &l_error);

	if (FAILED(l_result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: Can't serialize RootSignature!");
		return false;
	}

	l_result = DX12RenderingSystemComponent::get().m_device->CreateRootSignature(0, l_signature->GetBufferPointer(), l_signature->GetBufferSize(), IID_PPV_ARGS(&DXRPC->m_rootSignature));

	if (FAILED(l_result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: Can't create Root Signature!");
		return false;
	}

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingSystem: RootSignature has been created.");

	return true;
}

bool DX12RenderingSystemNS::createPSO(DX12RenderPassComponent* DXRPC, DX12ShaderProgramComponent* DXSPC)
{
	D3D12_INPUT_ELEMENT_DESC l_polygonLayout[5];
	unsigned int l_numElements;

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

	auto l_result = DX12RenderingSystemComponent::get().m_device->CreateGraphicsPipelineState(&DXRPC->m_PSODesc, IID_PPV_ARGS(&DXRPC->m_PSO));

	if (FAILED(l_result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: Can't create PSO!");
		return false;
	}

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DX12RenderingSystem: PSO has been created.");

	return true;
}

bool DX12RenderingSystemNS::initializeDX12MeshDataComponent(DX12MeshDataComponent* rhs)
{
	if (rhs->m_objectStatus == ObjectStatus::ALIVE)
	{
		return true;
	}
	else
	{
		summitGPUData(rhs);

		rhs->m_objectStatus = ObjectStatus::ALIVE;

		return true;
	}
}

bool DX12RenderingSystemNS::initializeDX12TextureDataComponent(DX12TextureDataComponent * rhs)
{
	if (rhs->m_objectStatus == ObjectStatus::ALIVE)
	{
		return true;
	}
	else
	{
		if (rhs->m_textureDataDesc.usageType == TextureUsageType::INVISIBLE)
		{
			g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "DX12RenderingSystem: try to generate DX12TextureDataComponent for TextureUsageType::INVISIBLE type!");
			return false;
		}
		else
		{
			if (rhs->m_textureData.size() > 0)
			{
				summitGPUData(rhs);

				rhs->m_objectStatus = ObjectStatus::ALIVE;

				if (rhs->m_textureDataDesc.usageType != TextureUsageType::RENDER_TARGET)
				{
					// @TODO: release raw data in heap memory
				}

				return true;
			}
			else
			{
				g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_WARNING, "DX12RenderingSystem: try to generate DX12TextureDataComponent without raw data!");
				return false;
			}
		}
	}
}

bool DX12RenderingSystemNS::summitGPUData(DX12MeshDataComponent * rhs)
{
	// Set up the description of the static vertex buffer.

	// Give the subresource structure a pointer to the vertex data.

	// Now create the vertex buffer.

	// Set up the description of the static index buffer.

	// Give the subresource structure a pointer to the index data.

	// Create the index buffer.

	rhs->m_objectStatus = ObjectStatus::ALIVE;

	m_initializedDXMDC.emplace(rhs->m_parentEntity, rhs);

	return true;
}

bool DX12RenderingSystemNS::summitGPUData(DX12TextureDataComponent * rhs)
{
	// set texture formats
	DXGI_FORMAT l_internalFormat = DXGI_FORMAT_UNKNOWN;

	// @TODO: Unified internal format
	// Setup the description of the texture.
	// Different than OpenGL, DX's format didn't allow a RGB structure for 8-bits and 16-bits per channel
	if (rhs->m_textureDataDesc.usageType == TextureUsageType::ALBEDO)
	{
		l_internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	}
	else
	{
		if (rhs->m_textureDataDesc.pixelDataType == TexturePixelDataType::UNSIGNED_BYTE)
		{
			switch (rhs->m_textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::RED: l_internalFormat = DXGI_FORMAT_R8_UNORM; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R8G8_UNORM; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
			default: break;
			}
		}
		else if (rhs->m_textureDataDesc.pixelDataType == TexturePixelDataType::FLOAT)
		{
			switch (rhs->m_textureDataDesc.pixelDataFormat)
			{
			case TexturePixelDataFormat::RED: l_internalFormat = DXGI_FORMAT_R16_FLOAT; break;
			case TexturePixelDataFormat::RG: l_internalFormat = DXGI_FORMAT_R16G16_FLOAT; break;
			case TexturePixelDataFormat::RGB: l_internalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
			case TexturePixelDataFormat::RGBA: l_internalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
			default: break;
			}
		}
	}

	unsigned int textureMipLevels = 1;
	unsigned int miscFlags = 0;
	if (rhs->m_textureDataDesc.magFilterMethod == TextureFilterMethod::LINEAR_MIPMAP_LINEAR)
	{
		textureMipLevels = 0;
	}

	D3D12_RESOURCE_DESC D3DTextureDesc = {};
	D3DTextureDesc.MipLevels = 1;
	D3DTextureDesc.Format = l_internalFormat;
	D3DTextureDesc.Width = rhs->m_textureDataDesc.height;
	D3DTextureDesc.Height = rhs->m_textureDataDesc.width;
	D3DTextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	D3DTextureDesc.DepthOrArraySize = 1;
	D3DTextureDesc.SampleDesc.Count = 1;
	D3DTextureDesc.SampleDesc.Quality = 0;
	D3DTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	if (rhs->m_textureDataDesc.usageType != TextureUsageType::RENDER_TARGET)
	{
		D3DTextureDesc.SampleDesc.Quality = 0;
	}

	unsigned int SRVMipLevels = -1;
	if (rhs->m_textureDataDesc.usageType == TextureUsageType::RENDER_TARGET)
	{
		SRVMipLevels = 1;
	}

	// Create the empty texture.
	HRESULT hResult;
	hResult = DX12RenderingSystemComponent::get().m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&D3DTextureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&rhs->m_texture));

	if (FAILED(hResult))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: can't create texture!");
		return false;
	}

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(rhs->m_texture, 0, 1);

	ID3D12Resource* textureUploadHeap;

	// Create the GPU upload buffer.
	hResult = DX12RenderingSystemComponent::get().m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&textureUploadHeap));

	if (FAILED(hResult))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: can't create upload buffer!");
		return false;
	}

	// Copy data to the intermediate upload heap and then schedule a copy
	// from the upload heap to the Texture2D.
	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = rhs->m_textureData[0];
	textureData.RowPitch = rhs->m_textureDataDesc.width * ((unsigned int)rhs->m_textureDataDesc.pixelDataFormat + 1);
	textureData.SlicePitch = textureData.RowPitch * rhs->m_textureDataDesc.height;

	auto l_commandList = beginSingleTimeCommands();

	UpdateSubresources(l_commandList, rhs->m_texture, textureUploadHeap, 0, 0, 1, &textureData);

	l_commandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(rhs->m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	// Describe and create a shader resource view (SRV) heap for the texture.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hResult = DX12RenderingSystemComponent::get().m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&rhs->m_SRVHeap));

	if (FAILED(hResult))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DX12RenderingSystem: can't create SRV heap!");
		return false;
	}

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = D3DTextureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = SRVMipLevels;
	DX12RenderingSystemComponent::get().m_device->CreateShaderResourceView(rhs->m_texture, &srvDesc, rhs->m_SRVHeap->GetCPUDescriptorHandleForHeapStart());

	endSingleTimeCommands(l_commandList);

	rhs->m_objectStatus = ObjectStatus::ALIVE;

	m_initializedDXTDC.emplace(rhs->m_parentEntity, rhs);

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_VERBOSE, "DX12RenderingSystem: SRV " + InnoUtility::pointerToString(rhs->m_SRV) + " is initialized.");

	return true;
}

void DX12RenderingSystemNS::recordDrawCall(EntityID rhs)
{
	auto l_MDC = g_pCoreSystem->getAssetSystem()->getMeshDataComponent(rhs);
	if (l_MDC)
	{
		recordDrawCall(l_MDC);
	}
}

void DX12RenderingSystemNS::recordDrawCall(MeshDataComponent * MDC)
{
	auto l_DXMDC = DX12RenderingSystemNS::getDX12MeshDataComponent(MDC->m_parentEntity);
	if (l_DXMDC)
	{
		if (MDC->m_objectStatus == ObjectStatus::ALIVE && l_DXMDC->m_objectStatus == ObjectStatus::ALIVE)
		{
			recordDrawCall(MDC->m_indicesSize, l_DXMDC);
		}
	}
}

void DX12RenderingSystemNS::recordDrawCall(size_t indicesSize, DX12MeshDataComponent * DXMDC)
{
	unsigned int stride;
	unsigned int offset;

	// Set vertex buffer stride and offset.
	stride = sizeof(Vertex);
	offset = 0;
}

void DX12RenderingSystemNS::updateShaderParameter(ShaderType shaderType, unsigned int startSlot, ID3D12Resource* CBuffer, size_t size, void* parameterValue)
{
}

void DX12RenderingSystemNS::cleanRTV(vec4 color, ID3D12Resource* RTV)
{
	float l_color[4];

	// Setup the color to clear the buffer to.
	l_color[0] = color.x;
	l_color[1] = color.y;
	l_color[2] = color.z;
	l_color[3] = color.w;
}

void DX12RenderingSystemNS::cleanDSV(ID3D12Resource* DSV)
{
}