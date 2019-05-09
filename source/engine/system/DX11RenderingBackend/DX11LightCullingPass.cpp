#include "DX11LightCullingPass.h"
#include "DX11RenderingSystemUtilities.h"

#include "DX11OpaquePass.h"

#include "../../component/DX11RenderingSystemComponent.h"
#include "../../component/RenderingFrontendSystemComponent.h"

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

using namespace DX11RenderingSystemNS;

INNO_PRIVATE_SCOPE DX11LightCullingPass
{
	bool initializeShaders();
	bool createGridFrustumsBuffer();
	bool createLightIndexCountBuffer();
	bool createLightIndexListBuffer();
	bool createLightGridDXTDC();

	bool calculateFrustums();
	bool cullLights();

	DX11RenderPassComponent* m_DXRPC;

	DX11ShaderProgramComponent* m_tileFrustumDXSPC;
	DX11ShaderProgramComponent* m_lightCullingDXSPC;

	ShaderFilePaths m_tileFrustumShaderFilePaths = { "" , "", "", "DX11//tileFrustumCompute.hlsl" };
	ShaderFilePaths m_lightCullingShaderFilePaths = { "" , "", "", "DX11//lightCullingCompute.hlsl" };

	DX11TextureDataComponent* m_lightGridDXTDC;
	DX11TextureDataComponent* m_debugDXTDC;

	EntityID m_entityID;
	const unsigned int m_tileSize = 16;
	const unsigned int m_numThreadPerGroup = 16;
	TVec4<unsigned int> m_tileFrustumNumThreads;
	TVec4<unsigned int> m_tileFrustumNumThreadGroups;
	TVec4<unsigned int> m_lightCullingNumThreads;
	TVec4<unsigned int> m_lightCullingNumThreadGroups;
}

bool DX11LightCullingPass::createGridFrustumsBuffer()
{
	auto l_viewportSize = g_pCoreSystem->getVisionSystem()->getRenderingFrontend()->getScreenResolution();

	auto l_numThreadsX = std::ceil(l_viewportSize.x / m_tileSize);
	auto l_numThreadsY = std::ceil(l_viewportSize.y / m_tileSize);

	auto l_numThreadGroupsX = std::ceil(l_numThreadsX / m_numThreadPerGroup);
	auto l_numThreadGroupsY = std::ceil(l_numThreadsY / m_numThreadPerGroup);

	m_tileFrustumNumThreads = TVec4<unsigned int>((unsigned int)l_numThreadsX, (unsigned int)l_numThreadsY, 1, 0);
	m_tileFrustumNumThreadGroups = TVec4<unsigned int>((unsigned int)l_numThreadGroupsX, (unsigned int)l_numThreadGroupsY, 1, 0);

	auto l_elementCount = m_tileFrustumNumThreads.x * m_tileFrustumNumThreads.y;

	DX11RenderingSystemComponent::get().m_gridFrustumsStructuredBuffer.m_StructuredBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	DX11RenderingSystemComponent::get().m_gridFrustumsStructuredBuffer.m_StructuredBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	DX11RenderingSystemComponent::get().m_gridFrustumsStructuredBuffer.m_StructuredBufferDesc.StructureByteStride = 64;
	DX11RenderingSystemComponent::get().m_gridFrustumsStructuredBuffer.m_StructuredBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	DX11RenderingSystemComponent::get().m_gridFrustumsStructuredBuffer.m_StructuredBufferDesc.ByteWidth = l_elementCount * 64;
	DX11RenderingSystemComponent::get().m_gridFrustumsStructuredBuffer.elementCount = l_elementCount;

	createStructuredBuffer(nullptr, DX11RenderingSystemComponent::get().m_gridFrustumsStructuredBuffer);

	return true;
}

bool DX11LightCullingPass::createLightIndexCountBuffer()
{
	DX11RenderingSystemComponent::get().m_lightListIndexCounterStructuredBuffer.m_StructuredBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	DX11RenderingSystemComponent::get().m_lightListIndexCounterStructuredBuffer.m_StructuredBufferDesc.ByteWidth = sizeof(uint32_t);
	DX11RenderingSystemComponent::get().m_lightListIndexCounterStructuredBuffer.m_StructuredBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	DX11RenderingSystemComponent::get().m_lightListIndexCounterStructuredBuffer.m_StructuredBufferDesc.StructureByteStride = sizeof(uint32_t);
	DX11RenderingSystemComponent::get().m_lightListIndexCounterStructuredBuffer.m_StructuredBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	DX11RenderingSystemComponent::get().m_lightListIndexCounterStructuredBuffer.elementCount = 1;

	auto l_initialIndexCount = 1;
	createStructuredBuffer(&l_initialIndexCount, DX11RenderingSystemComponent::get().m_lightListIndexCounterStructuredBuffer);

	return true;
}

bool DX11LightCullingPass::createLightIndexListBuffer()
{
	auto l_averangeOverlapLight = 64;

	auto l_viewportSize = g_pCoreSystem->getVisionSystem()->getRenderingFrontend()->getScreenResolution();

	auto l_numThreadGroupsX = std::ceil(l_viewportSize.x / m_tileSize);
	auto l_numThreadGroupsY = std::ceil(l_viewportSize.y / m_tileSize);

	m_lightCullingNumThreadGroups = TVec4<unsigned int>((unsigned int)l_numThreadGroupsX, (unsigned int)l_numThreadGroupsY, 1, 0);
	m_lightCullingNumThreads = TVec4<unsigned int>(m_tileSize, m_tileSize, 1, 0);

	auto l_elementCount = m_lightCullingNumThreadGroups.x * m_lightCullingNumThreadGroups.y * l_averangeOverlapLight;

	DX11RenderingSystemComponent::get().m_lightIndexListStructuredBuffer.m_StructuredBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	DX11RenderingSystemComponent::get().m_lightIndexListStructuredBuffer.m_StructuredBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	DX11RenderingSystemComponent::get().m_lightIndexListStructuredBuffer.m_StructuredBufferDesc.StructureByteStride = sizeof(uint32_t);
	DX11RenderingSystemComponent::get().m_lightIndexListStructuredBuffer.m_StructuredBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	DX11RenderingSystemComponent::get().m_lightIndexListStructuredBuffer.m_StructuredBufferDesc.ByteWidth = l_elementCount * sizeof(uint32_t);
	DX11RenderingSystemComponent::get().m_lightIndexListStructuredBuffer.elementCount = l_elementCount;

	createStructuredBuffer(nullptr, DX11RenderingSystemComponent::get().m_lightIndexListStructuredBuffer);

	return true;
}

bool DX11LightCullingPass::createLightGridDXTDC()
{
	m_lightGridDXTDC = addDX11TextureDataComponent();
	m_lightGridDXTDC->m_textureDataDesc = DX11RenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc;
	m_lightGridDXTDC->m_textureDataDesc.width = m_lightCullingNumThreadGroups.x;
	m_lightGridDXTDC->m_textureDataDesc.height = m_lightCullingNumThreadGroups.y;
	m_lightGridDXTDC->m_textureDataDesc.usageType = TextureUsageType::RAW_IMAGE;
	m_lightGridDXTDC->m_textureDataDesc.pixelDataFormat = TexturePixelDataFormat::RG;
	m_lightGridDXTDC->m_textureDataDesc.pixelDataType = TexturePixelDataType::UINT32;
	m_lightGridDXTDC->m_textureData = { nullptr };
	initializeDX11TextureDataComponent(m_lightGridDXTDC);

	return true;
}

bool DX11LightCullingPass::initialize()
{
	m_entityID = InnoMath::createEntityID();

	m_DXRPC = addDX11RenderPassComponent(m_entityID, "LightCullingPassDXRPC\\");

	m_DXRPC->m_renderPassDesc = DX11RenderingSystemComponent::get().m_deferredRenderPassDesc;
	m_DXRPC->m_renderPassDesc.RTNumber = 1;
	m_DXRPC->m_renderPassDesc.useDepthAttachment = false;
	m_DXRPC->m_renderPassDesc.useStencilAttachment = false;

	m_DXRPC->m_rasterizerDesc.AntialiasedLineEnable = false;
	m_DXRPC->m_rasterizerDesc.CullMode = D3D11_CULL_NONE;
	m_DXRPC->m_rasterizerDesc.DepthBias = 0;
	m_DXRPC->m_rasterizerDesc.DepthBiasClamp = 0.0f;
	m_DXRPC->m_rasterizerDesc.DepthClipEnable = true;
	m_DXRPC->m_rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	m_DXRPC->m_rasterizerDesc.FrontCounterClockwise = false;
	m_DXRPC->m_rasterizerDesc.MultisampleEnable = false;
	m_DXRPC->m_rasterizerDesc.ScissorEnable = false;
	m_DXRPC->m_rasterizerDesc.SlopeScaledDepthBias = 0.0f;

	initializeShaders();
	initializeDX11RenderPassComponent(m_DXRPC);

	createGridFrustumsBuffer();

	createLightIndexListBuffer();
	createLightGridDXTDC();

	m_debugDXTDC = addDX11TextureDataComponent();
	m_debugDXTDC->m_textureDataDesc = DX11RenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc;
	m_debugDXTDC->m_textureDataDesc.usageType = TextureUsageType::RAW_IMAGE;
	m_debugDXTDC->m_textureDataDesc.pixelDataFormat = TexturePixelDataFormat::RGBA;
	m_debugDXTDC->m_textureDataDesc.pixelDataType = TexturePixelDataType::FLOAT16;
	m_debugDXTDC->m_textureData = { nullptr };
	initializeDX11TextureDataComponent(m_debugDXTDC);

	return true;
}

bool DX11LightCullingPass::initializeShaders()
{
	m_tileFrustumDXSPC = addDX11ShaderProgramComponent(m_entityID);
	m_lightCullingDXSPC = addDX11ShaderProgramComponent(m_entityID);

	initializeDX11ShaderProgramComponent(m_tileFrustumDXSPC, m_tileFrustumShaderFilePaths);
	initializeDX11ShaderProgramComponent(m_lightCullingDXSPC, m_lightCullingShaderFilePaths);

	return true;
}

bool DX11LightCullingPass::calculateFrustums()
{
	RenderingFrontendSystemComponent::get().m_dispatchParamsGPUData.numThreadGroups = m_tileFrustumNumThreadGroups;
	RenderingFrontendSystemComponent::get().m_dispatchParamsGPUData.numThreads = m_tileFrustumNumThreads;
	updateConstantBuffer(DX11RenderingSystemComponent::get().m_dispatchParamsConstantBuffer, &RenderingFrontendSystemComponent::get().m_dispatchParamsGPUData);

	activateShader(m_tileFrustumDXSPC);

	bindConstantBuffer(ShaderType::COMPUTE, 0, DX11RenderingSystemComponent::get().m_dispatchParamsConstantBuffer);
	bindConstantBuffer(ShaderType::COMPUTE, 1, DX11RenderingSystemComponent::get().m_skyConstantBuffer);
	bindStructuredBufferForWrite(ShaderType::COMPUTE, 0, DX11RenderingSystemComponent::get().m_gridFrustumsStructuredBuffer);

	DX11RenderingSystemComponent::get().m_deviceContext->Dispatch(m_tileFrustumNumThreadGroups.x, m_tileFrustumNumThreadGroups.y, m_tileFrustumNumThreadGroups.z);

	unbindStructuredBufferForWrite(ShaderType::COMPUTE, 0);

	return true;
}

bool DX11LightCullingPass::cullLights()
{
	RenderingFrontendSystemComponent::get().m_dispatchParamsGPUData.numThreadGroups = m_lightCullingNumThreadGroups;
	RenderingFrontendSystemComponent::get().m_dispatchParamsGPUData.numThreads = m_lightCullingNumThreads;
	updateConstantBuffer(DX11RenderingSystemComponent::get().m_dispatchParamsConstantBuffer, &RenderingFrontendSystemComponent::get().m_dispatchParamsGPUData);

	activateShader(m_lightCullingDXSPC);

	bindConstantBuffer(ShaderType::COMPUTE, 0, DX11RenderingSystemComponent::get().m_dispatchParamsConstantBuffer);
	bindConstantBuffer(ShaderType::COMPUTE, 1, DX11RenderingSystemComponent::get().m_cameraConstantBuffer);
	bindConstantBuffer(ShaderType::COMPUTE, 2, DX11RenderingSystemComponent::get().m_skyConstantBuffer);
	bindConstantBuffer(ShaderType::COMPUTE, 3, DX11RenderingSystemComponent::get().m_pointLightConstantBuffer);

	bindTextureForRead(ShaderType::COMPUTE, 0, DX11OpaquePass::getDX11RPC()->m_depthStencilDXTDC);
	bindStructuredBufferForRead(ShaderType::COMPUTE, 1, DX11RenderingSystemComponent::get().m_gridFrustumsStructuredBuffer);

	bindStructuredBufferForWrite(ShaderType::COMPUTE, 0, DX11RenderingSystemComponent::get().m_lightListIndexCounterStructuredBuffer);
	bindStructuredBufferForWrite(ShaderType::COMPUTE, 1, DX11RenderingSystemComponent::get().m_lightIndexListStructuredBuffer);
	bindTextureForWrite(ShaderType::COMPUTE, 2, m_lightGridDXTDC);
	bindTextureForWrite(ShaderType::COMPUTE, 3, m_debugDXTDC);

	DX11RenderingSystemComponent::get().m_deviceContext->Dispatch(m_lightCullingNumThreadGroups.x, m_lightCullingNumThreadGroups.y, m_lightCullingNumThreadGroups.z);

	unbindTextureForRead(ShaderType::COMPUTE, 0);
	unbindStructuredBufferForRead(ShaderType::COMPUTE, 1);

	unbindStructuredBufferForWrite(ShaderType::COMPUTE, 0);
	unbindStructuredBufferForWrite(ShaderType::COMPUTE, 1);
	unbindTextureForWrite(ShaderType::COMPUTE, 2);
	unbindTextureForWrite(ShaderType::COMPUTE, 3);

	return true;
}

bool DX11LightCullingPass::update()
{
	activateRenderPass(m_DXRPC);

	createLightIndexCountBuffer();

	calculateFrustums();
	cullLights();

	destroyStructuredBuffer(DX11RenderingSystemComponent::get().m_lightListIndexCounterStructuredBuffer);

	return true;
}

bool DX11LightCullingPass::resize()
{
	return true;
}

bool DX11LightCullingPass::reloadShaders()
{
	return true;
}

DX11RenderPassComponent * DX11LightCullingPass::getDX11RPC()
{
	return m_DXRPC;
}

DX11TextureDataComponent* DX11LightCullingPass::getLightGrid()
{
	return m_lightGridDXTDC;
}

DX11TextureDataComponent * DX11LightCullingPass::getHeatMap()
{
	return m_debugDXTDC;
}