#include "SSAOPass.h"
#include "DefaultGPUBuffers.h"

#include "OpaquePass.h"

#include "../../Engine/ModuleManager/IModuleManager.h"

INNO_ENGINE_API extern IModuleManager* g_pModuleManager;

using namespace DefaultGPUBuffers;

namespace SSAOPass
{
	RenderPassDataComponent* m_RPDC;
	ShaderProgramComponent* m_SPC;
	SamplerDataComponent* m_SDC;
	SamplerDataComponent* m_SDC_RandomRot;

	unsigned int m_kernelSize = 64;
	std::vector<vec4> m_SSAOKernel;
	std::vector<vec4> m_SSAONoise;

	GPUBufferDataComponent* m_SSAOKernelGPUBuffer;
	TextureDataComponent* m_SSAONoiseTDC;
}

bool SSAOPass::Setup()
{
	m_SPC = g_pModuleManager->getRenderingServer()->AddShaderProgramComponent("SSAONoisePass/");

	m_SPC->m_ShaderFilePaths.m_VSPath = "2DImageProcess.vert/";
	m_SPC->m_ShaderFilePaths.m_PSPath = "SSAONoisePass.frag/";

	g_pModuleManager->getRenderingServer()->InitializeShaderProgramComponent(m_SPC);

	m_RPDC = g_pModuleManager->getRenderingServer()->AddRenderPassDataComponent("SSAONoisePass/");

	auto l_RenderPassDesc = g_pModuleManager->getRenderingFrontend()->getDefaultRenderPassDesc();

	l_RenderPassDesc.m_RenderTargetCount = 1;

	l_RenderPassDesc.m_GraphicsPipelineDesc.m_RasterizerDesc.m_UseCulling = false;

	m_RPDC->m_RenderPassDesc = l_RenderPassDesc;

	m_RPDC->m_ResourceBinderLayoutDescs.resize(7);
	m_RPDC->m_ResourceBinderLayoutDescs[0].m_ResourceBinderType = ResourceBinderType::Buffer;
	m_RPDC->m_ResourceBinderLayoutDescs[0].m_GlobalSlot = 0;
	m_RPDC->m_ResourceBinderLayoutDescs[0].m_LocalSlot = 0;

	m_RPDC->m_ResourceBinderLayoutDescs[1].m_ResourceBinderType = ResourceBinderType::Buffer;
	m_RPDC->m_ResourceBinderLayoutDescs[1].m_GlobalSlot = 1;
	m_RPDC->m_ResourceBinderLayoutDescs[1].m_LocalSlot = 10;

	m_RPDC->m_ResourceBinderLayoutDescs[2].m_ResourceBinderType = ResourceBinderType::Image;
	m_RPDC->m_ResourceBinderLayoutDescs[2].m_GlobalSlot = 2;
	m_RPDC->m_ResourceBinderLayoutDescs[2].m_LocalSlot = 0;
	m_RPDC->m_ResourceBinderLayoutDescs[2].m_IsRanged = true;

	m_RPDC->m_ResourceBinderLayoutDescs[3].m_ResourceBinderType = ResourceBinderType::Image;
	m_RPDC->m_ResourceBinderLayoutDescs[3].m_GlobalSlot = 3;
	m_RPDC->m_ResourceBinderLayoutDescs[3].m_LocalSlot = 1;
	m_RPDC->m_ResourceBinderLayoutDescs[3].m_IsRanged = true;

	m_RPDC->m_ResourceBinderLayoutDescs[4].m_ResourceBinderType = ResourceBinderType::Image;
	m_RPDC->m_ResourceBinderLayoutDescs[4].m_GlobalSlot = 4;
	m_RPDC->m_ResourceBinderLayoutDescs[4].m_LocalSlot = 2;
	m_RPDC->m_ResourceBinderLayoutDescs[4].m_IsRanged = true;

	m_RPDC->m_ResourceBinderLayoutDescs[5].m_ResourceBinderType = ResourceBinderType::Sampler;
	m_RPDC->m_ResourceBinderLayoutDescs[5].m_GlobalSlot = 5;
	m_RPDC->m_ResourceBinderLayoutDescs[5].m_LocalSlot = 0;
	m_RPDC->m_ResourceBinderLayoutDescs[5].m_IsRanged = true;

	m_RPDC->m_ResourceBinderLayoutDescs[6].m_ResourceBinderType = ResourceBinderType::Sampler;
	m_RPDC->m_ResourceBinderLayoutDescs[6].m_GlobalSlot = 6;
	m_RPDC->m_ResourceBinderLayoutDescs[6].m_LocalSlot = 1;
	m_RPDC->m_ResourceBinderLayoutDescs[6].m_IsRanged = true;

	m_RPDC->m_ShaderProgram = m_SPC;

	g_pModuleManager->getRenderingServer()->InitializeRenderPassDataComponent(m_RPDC);

	m_SDC = g_pModuleManager->getRenderingServer()->AddSamplerDataComponent("SSAONoisePass/");

	g_pModuleManager->getRenderingServer()->InitializeSamplerDataComponent(m_SDC);

	m_SDC_RandomRot = g_pModuleManager->getRenderingServer()->AddSamplerDataComponent("SSAONoisePass_RandomRot/");

	m_SDC_RandomRot->m_SamplerDesc.m_MinFilterMethod = TextureFilterMethod::Nearest;
	m_SDC_RandomRot->m_SamplerDesc.m_MagFilterMethod = TextureFilterMethod::Nearest;
	m_SDC_RandomRot->m_SamplerDesc.m_WrapMethodU = TextureWrapMethod::Repeat;
	m_SDC_RandomRot->m_SamplerDesc.m_WrapMethodV = TextureWrapMethod::Repeat;

	g_pModuleManager->getRenderingServer()->InitializeSamplerDataComponent(m_SDC_RandomRot);

	// Kernel
	std::uniform_real_distribution<float> l_randomFloats(0.0f, 1.0f);
	std::default_random_engine l_generator;

	m_SSAOKernel.reserve(m_kernelSize);

	for (unsigned int i = 0; i < m_kernelSize; ++i)
	{
		auto l_sample = vec4(l_randomFloats(l_generator) * 2.0f - 1.0f, l_randomFloats(l_generator) * 2.0f - 1.0f, l_randomFloats(l_generator), 0.0f);
		l_sample = l_sample.normalize();
		l_sample = l_sample * l_randomFloats(l_generator);
		float l_scale = float(i) / float(m_kernelSize);

		// scale samples s.t. they're more aligned to center of kernel
		auto l_alpha = l_scale * l_scale;
		l_scale = 0.1f + 0.9f * l_alpha;
		l_sample.x = l_sample.x * l_scale;
		l_sample.y = l_sample.y * l_scale;
		m_SSAOKernel.emplace_back(l_sample);
	}

	m_SSAOKernelGPUBuffer = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("SSAOKernel/");
	m_SSAOKernelGPUBuffer->m_ElementSize = sizeof(vec4);
	m_SSAOKernelGPUBuffer->m_ElementCount = m_kernelSize;
	m_SSAOKernelGPUBuffer->m_InitialData = &m_SSAOKernel[0];
	m_SSAOKernelGPUBuffer->m_BindingPoint = 10;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_SSAOKernelGPUBuffer);

	// Noise
	auto l_textureSize = 4;

	m_SSAONoise.reserve(l_textureSize * l_textureSize);

	for (size_t i = 0; i < m_SSAONoise.capacity(); i++)
	{
		// rotate around z-axis (in tangent space)
		auto noise = vec4(l_randomFloats(l_generator) * 2.0f - 1.0f, l_randomFloats(l_generator) * 2.0f - 1.0f, 0.0f, 0.0f);
		noise = noise.normalize();
		m_SSAONoise.push_back(noise);
	}

	m_SSAONoiseTDC = g_pModuleManager->getRenderingServer()->AddTextureDataComponent("SSAONoise/");

	m_SSAONoiseTDC->m_textureDataDesc.SamplerType = TextureSamplerType::Sampler2D;
	m_SSAONoiseTDC->m_textureDataDesc.UsageType = TextureUsageType::Normal;
	m_SSAONoiseTDC->m_textureDataDesc.PixelDataFormat = TexturePixelDataFormat::RGBA;
	m_SSAONoiseTDC->m_textureDataDesc.MinFilterMethod = TextureFilterMethod::Nearest;
	m_SSAONoiseTDC->m_textureDataDesc.MagFilterMethod = TextureFilterMethod::Nearest;
	m_SSAONoiseTDC->m_textureDataDesc.WrapMethod = TextureWrapMethod::Repeat;
	m_SSAONoiseTDC->m_textureDataDesc.Width = l_textureSize;
	m_SSAONoiseTDC->m_textureDataDesc.Height = l_textureSize;
	m_SSAONoiseTDC->m_textureDataDesc.PixelDataType = TexturePixelDataType::FLOAT32;

	std::vector<float> l_pixelBuffer;
	auto l_containerSize = m_SSAONoise.size() * 4;
	l_pixelBuffer.reserve(l_containerSize);

	std::for_each(m_SSAONoise.begin(), m_SSAONoise.end(), [&](vec4 val)
	{
		l_pixelBuffer.emplace_back(val.x);
		l_pixelBuffer.emplace_back(val.y);
		l_pixelBuffer.emplace_back(val.z);
		l_pixelBuffer.emplace_back(val.w);
	});

	m_SSAONoiseTDC->m_textureData = &l_pixelBuffer[0];

	g_pModuleManager->getRenderingServer()->InitializeTextureDataComponent(m_SSAONoiseTDC);

	return true;
}

bool SSAOPass::Initialize()
{
	return true;
}

bool SSAOPass::PrepareCommandList()
{
	auto l_CameraGBDC = GetGPUBufferDataComponent(GPUBufferUsageType::Camera);

	g_pModuleManager->getRenderingServer()->CommandListBegin(m_RPDC, 0);
	g_pModuleManager->getRenderingServer()->BindRenderPassDataComponent(m_RPDC);
	g_pModuleManager->getRenderingServer()->CleanRenderTargets(m_RPDC);
	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC, ShaderStage::Pixel, m_SDC->m_ResourceBinder, 5, 0);
	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC, ShaderStage::Pixel, m_SDC_RandomRot->m_ResourceBinder, 6, 1);

	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC, ShaderStage::Pixel, l_CameraGBDC->m_ResourceBinder, 0, 0, Accessibility::ReadOnly, false, 0, l_CameraGBDC->m_TotalSize);
	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC, ShaderStage::Pixel, m_SSAOKernelGPUBuffer->m_ResourceBinder, 1, 10, Accessibility::ReadOnly, false, 0, m_SSAOKernelGPUBuffer->m_TotalSize);
	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC, ShaderStage::Pixel, OpaquePass::GetRPDC()->m_RenderTargetsResourceBinders[0], 2, 0);
	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC, ShaderStage::Pixel, OpaquePass::GetRPDC()->m_RenderTargetsResourceBinders[1], 3, 1);
	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC, ShaderStage::Pixel, m_SSAONoiseTDC->m_ResourceBinder, 4, 2);

	auto l_mesh = g_pModuleManager->getRenderingFrontend()->getMeshDataComponent(MeshShapeType::Quad);

	g_pModuleManager->getRenderingServer()->DispatchDrawCall(m_RPDC, l_mesh);

	g_pModuleManager->getRenderingServer()->DeactivateResourceBinder(m_RPDC, ShaderStage::Pixel, OpaquePass::GetRPDC()->m_RenderTargetsResourceBinders[0], 2, 0);
	g_pModuleManager->getRenderingServer()->DeactivateResourceBinder(m_RPDC, ShaderStage::Pixel, OpaquePass::GetRPDC()->m_RenderTargetsResourceBinders[1], 3, 1);
	g_pModuleManager->getRenderingServer()->DeactivateResourceBinder(m_RPDC, ShaderStage::Pixel, m_SSAONoiseTDC->m_ResourceBinder, 4, 2);

	g_pModuleManager->getRenderingServer()->CommandListEnd(m_RPDC);

	return true;
}

bool SSAOPass::ExecuteCommandList()
{
	g_pModuleManager->getRenderingServer()->ExecuteCommandList(m_RPDC);

	g_pModuleManager->getRenderingServer()->WaitForFrame(m_RPDC);

	return true;
}

RenderPassDataComponent * SSAOPass::GetRPDC()
{
	return m_RPDC;
}

ShaderProgramComponent * SSAOPass::GetSPC()
{
	return m_SPC;
}