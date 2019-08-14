#include "GIBakePass.h"
#include "DefaultGPUBuffers.h"
#include "GIResolvePass.h"

#include "../../Engine/Common/InnoMathHelper.h"

#include "../../Engine/ModuleManager/IModuleManager.h"
INNO_ENGINE_API extern IModuleManager* g_pModuleManager;

#include "../../Engine/Core/IOService.h"

struct BrickCache
{
	vec4 pos;
	std::vector<Surfel> surfelCaches;
};

using namespace DefaultGPUBuffers;

namespace GIBakePass
{
	bool loadGIData();

	bool generateProbes();

	bool captureSurfels();
	bool drawCubemaps(unsigned int probeIndex, const mat4& p, const std::vector<mat4>& v);
	bool drawOpaquePass(unsigned int probeIndex, const mat4& p, const std::vector<mat4>& v);
	bool drawSkyVisibilityPass(unsigned int probeIndex, const mat4& p, const std::vector<mat4>& v);
	bool readBackSurfelCaches(unsigned int probeIndex);

	bool eliminateDuplicatedSurfels();
	bool generateBricks();

	bool serializeSurfels();
	bool serializeBricks();

	bool assignBrickFactorToProbesByGPU();
	bool drawBricks(unsigned int probeIndex, const mat4& p, const std::vector<mat4>& v);
	bool readBackBrickFactors(unsigned int probeIndex);

	bool assignBrickFactorToProbesByCPU();

	bool serializeBrickFactors();
	bool serializeProbes();

	const unsigned int m_probeMapResolution = 1024;
	const float m_probeHeightOffset = 4.0f;
	const unsigned int m_probeMapSamplingInterval = 128;
	const unsigned int m_captureResolution = 64;
	const unsigned int m_sampleCountPerFace = m_captureResolution * m_captureResolution;
	const vec4 m_brickSize = vec4(8.0f, 8.0f, 8.0f, 0.0f);
	vec4 m_irradianceVolumeRange;

	TextureDataComponent* m_testSampleCubemap;
	TextureDataComponent* m_testSample3DTexture;

	RenderPassDataComponent* m_RPDC_Probe;
	ShaderProgramComponent* m_SPC_Probe;

	RenderPassDataComponent* m_RPDC_Surfel;
	ShaderProgramComponent* m_SPC_Surfel;
	SamplerDataComponent* m_SDC_Surfel;
	TextureDataComponent* m_testSurfelTexture;

	RenderPassDataComponent* m_RPDC_BrickFactor;
	ShaderProgramComponent* m_SPC_BrickFactor;

	std::vector<Probe> m_probes;
	std::vector<Surfel> m_surfels;
	std::vector<BrickCache> m_brickCaches;
	std::vector<Brick> m_bricks;
	std::vector<BrickFactor> m_brickFactors;

	RenderPassDataComponent* m_RPDC_Relight;
	ShaderProgramComponent* m_SPC_Relight;
	SamplerDataComponent* m_SDC_Relight;

	bool m_IsSurfelLoaded = false;
	bool m_IsBrickLoaded = false;
	bool m_IsBrickFactorLoaded = false;
	bool m_IsProbeLoaded = false;

	std::function<void()> f_sceneLoadingFinishCallback;
}

bool GIBakePass::loadGIData()
{
	auto l_filePath = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_currentSceneName = g_pModuleManager->getFileSystem()->getCurrentSceneName();

	std::ifstream l_surfelFile;
	l_surfelFile.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoSurfel", std::ios::binary);

	if (l_surfelFile.is_open())
	{
		IOService::deserializeVector(l_surfelFile, m_surfels);
		m_IsSurfelLoaded = true;
	}

	std::ifstream l_brickFile;
	l_brickFile.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoBrick", std::ios::binary);

	if (l_brickFile.is_open())
	{
		IOService::deserializeVector(l_brickFile, m_bricks);

		auto l_bricksCount = m_bricks.size();

		auto l_min = InnoMath::maxVec4<float>;
		auto l_max = InnoMath::minVec4<float>;

		for (size_t i = 0; i < l_bricksCount; i++)
		{
			l_min = InnoMath::elementWiseMin(l_min, m_bricks[i].boundBox.m_boundMin);
			l_max = InnoMath::elementWiseMax(l_max, m_bricks[i].boundBox.m_boundMax);
		}
		m_irradianceVolumeRange = l_max - l_min;

		m_IsBrickLoaded = true;
	}

	std::ifstream l_brickFactorFile;
	l_brickFactorFile.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoBrickFactor", std::ios::binary);

	if (l_brickFactorFile.is_open())
	{
		IOService::deserializeVector(l_brickFactorFile, m_brickFactors);
		m_IsBrickFactorLoaded = true;
	}

	std::ifstream l_probeFile;
	l_probeFile.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoProbe", std::ios::binary);

	if (l_probeFile.is_open())
	{
		IOService::deserializeVector(l_probeFile, m_probes);

		m_IsProbeLoaded = true;
	}

	return m_IsProbeLoaded && m_IsBrickFactorLoaded && m_IsBrickLoaded && m_IsSurfelLoaded;
}

bool GIBakePass::generateProbes()
{
	g_pModuleManager->getLogSystem()->Log(LogLevel::Verbose, "GIBakePass: Generate probes...");

	auto l_sceneAABB = g_pModuleManager->getPhysicsSystem()->getTotalSceneAABB();

	auto l_startPos = l_sceneAABB.m_boundMin;
	auto l_sceneCenter = l_sceneAABB.m_center;
	auto l_extendedAxisSize = l_sceneAABB.m_extend;

	auto l_p = InnoMath::generateOrthographicMatrix(-l_extendedAxisSize.x / 2.0f, l_extendedAxisSize.x / 2.0f, -l_extendedAxisSize.z / 2.0f, l_extendedAxisSize.z / 2.0f, -l_extendedAxisSize.y / 2.0f, l_extendedAxisSize.y / 2.0f);

	std::vector<mat4> l_GICameraGPUData(8);
	l_GICameraGPUData[0] = l_p;
	l_GICameraGPUData[1] = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 0.0f));
	l_GICameraGPUData[7] = InnoMath::generateIdentityMatrix<float>();

	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(GetGPUBufferDataComponent(GPUBufferUsageType::GICamera), l_GICameraGPUData);

	auto l_MeshGBDC = GetGPUBufferDataComponent(GPUBufferUsageType::Mesh);

	g_pModuleManager->getRenderingServer()->CommandListBegin(m_RPDC_Probe, 0);
	g_pModuleManager->getRenderingServer()->BindRenderPassDataComponent(m_RPDC_Probe);
	g_pModuleManager->getRenderingServer()->CleanRenderTargets(m_RPDC_Probe);
	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Probe, ShaderStage::Vertex, GetGPUBufferDataComponent(GPUBufferUsageType::GICamera)->m_ResourceBinder, 0, 10, Accessibility::ReadOnly);

	unsigned int l_offset = 0;

	auto l_totalDrawCallCount = g_pModuleManager->getRenderingFrontend()->getGIPassDrawCallCount();
	for (unsigned int i = 0; i < l_totalDrawCallCount; i++)
	{
		auto l_GIPassGPUData = g_pModuleManager->getRenderingFrontend()->getGIPassGPUData()[i];

		if (l_GIPassGPUData.mesh->m_objectStatus == ObjectStatus::Activated)
		{
			g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Probe, ShaderStage::Vertex, l_MeshGBDC->m_ResourceBinder, 1, 1, Accessibility::ReadOnly, l_offset, 1);

			g_pModuleManager->getRenderingServer()->DispatchDrawCall(m_RPDC_Probe, l_GIPassGPUData.mesh);
		}

		l_offset++;
	}

	g_pModuleManager->getRenderingServer()->CommandListEnd(m_RPDC_Probe);

	g_pModuleManager->getRenderingServer()->ExecuteCommandList(m_RPDC_Probe);

	g_pModuleManager->getRenderingServer()->WaitForFrame(m_RPDC_Probe);

	// Read back results and generate probes on x-z plate
	auto l_probePosTextureResults = g_pModuleManager->getRenderingServer()->ReadTextureBackToCPU(m_RPDC_Probe, m_RPDC_Probe->m_RenderTargets[0]);

	auto l_totalTextureSize = l_probePosTextureResults.size();

	m_probes.reserve(l_totalTextureSize);

	auto l_probesCountPerLine = m_probeMapResolution / m_probeMapSamplingInterval;

	for (size_t i = 0; i < l_probesCountPerLine; i++)
	{
		for (size_t j = 0; j < l_probesCountPerLine; j++)
		{
			auto l_currentIndex = i * m_probeMapSamplingInterval * m_probeMapResolution + j * m_probeMapSamplingInterval;
			auto l_textureResult = l_probePosTextureResults[l_currentIndex];

			Probe l_Probe;
			l_Probe.pos = l_textureResult;
			l_Probe.pos.y = l_textureResult.y + m_probeHeightOffset;

			m_probes.emplace_back(l_Probe);
		}
	}

	// Generate probes along the wall
	auto l_probesCount = m_probes.size();
	auto l_posIntervalX = std::abs((l_probePosTextureResults[0] - l_probePosTextureResults[m_probeMapSamplingInterval - 1]).x);
	auto l_posIntervalZ = std::abs((l_probePosTextureResults[0] - l_probePosTextureResults[m_probeMapResolution * m_probeMapSamplingInterval - 1]).z);

	for (size_t i = 0; i < l_probesCount; i++)
	{
		// Not the last one in all, not any one in last column, and not the last one each row
		if (i + 1 < l_probesCount && (i + l_probesCountPerLine) < l_probesCount && ((i + 1) % l_probesCountPerLine))
		{
			auto l_currentProbe = m_probes[i];
			auto l_nextRowProbe = m_probes[i + 1];
			auto l_nextColumnProbe = m_probes[i + l_probesCountPerLine];

			auto ddx = l_currentProbe.pos.y - l_nextRowProbe.pos.y;
			auto ddy = l_currentProbe.pos.y - l_nextColumnProbe.pos.y;

			if (ddx > l_posIntervalX)
			{
				auto l_verticalProbesCount = std::floor(ddx / l_posIntervalX);

				for (size_t k = 0; k < l_verticalProbesCount; k++)
				{
					Probe l_verticalProbe;
					l_verticalProbe.pos = l_nextRowProbe.pos;
					l_verticalProbe.pos.y += l_posIntervalX * (k + 1);

					m_probes.emplace_back(l_verticalProbe);
				}
			}
			if (ddx < -l_posIntervalX)
			{
				auto l_verticalProbesCount = std::floor(std::abs(ddx) / l_posIntervalX);

				for (size_t k = 0; k < l_verticalProbesCount; k++)
				{
					Probe l_verticalProbe;
					l_verticalProbe.pos = l_currentProbe.pos;
					l_verticalProbe.pos.y += l_posIntervalX * (k + 1);

					m_probes.emplace_back(l_verticalProbe);
				}
			}
			if (ddy > l_posIntervalZ)
			{
				auto l_verticalProbesCount = std::floor(ddy / l_posIntervalZ);

				for (size_t k = 0; k < l_verticalProbesCount; k++)
				{
					Probe l_verticalProbe;
					l_verticalProbe.pos = l_nextColumnProbe.pos;
					l_verticalProbe.pos.y += l_posIntervalZ * (k + 1);

					m_probes.emplace_back(l_verticalProbe);
				}
			}
			if (ddy < -l_posIntervalZ)
			{
				auto l_verticalProbesCount = std::floor(std::abs(ddy) / l_posIntervalZ);

				for (size_t k = 0; k < l_verticalProbesCount; k++)
				{
					Probe l_verticalProbe;
					l_verticalProbe.pos = l_currentProbe.pos;
					l_verticalProbe.pos.y += l_posIntervalZ * (k + 1);

					m_probes.emplace_back(l_verticalProbe);
				}
			}
		}
	}

	m_probes.shrink_to_fit();

	g_pModuleManager->getLogSystem()->Log(LogLevel::Success, "GIBakePass: ", m_probes.size(), " probes generated.");

	return true;
}

bool GIBakePass::captureSurfels()
{
	g_pModuleManager->getLogSystem()->Log(LogLevel::Verbose, "GIBakePass: Capture surfels...");

	auto l_cameraGPUData = g_pModuleManager->getRenderingFrontend()->getCameraGPUData();

	auto l_p = InnoMath::generatePerspectiveMatrix((90.0f / 180.0f) * PI<float>, 1.0f, l_cameraGPUData.zNear, l_cameraGPUData.zFar);

	auto l_rPX = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 0.0f));
	auto l_rNX = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(-1.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 0.0f));
	auto l_rPY = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, 1.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 0.0f));
	auto l_rNY = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 0.0f));
	auto l_rPZ = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 0.0f));
	auto l_rNZ = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, -1.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 0.0f));

	std::vector<mat4> l_v =
	{
		l_rPX, l_rNX, l_rPY, l_rNY, l_rPZ, l_rNZ
	};

	auto l_probeCount = m_probes.size();

	for (unsigned int i = 0; i < l_probeCount; i++)
	{
		drawCubemaps(i, l_p, l_v);
		readBackSurfelCaches(i);

		g_pModuleManager->getLogSystem()->Log(LogLevel::Verbose, "GIBakePass: Capture surfel ", (float)i * 100.0f / (float)l_probeCount, "%...");
	}

	g_pModuleManager->getLogSystem()->Log(LogLevel::Success, "GIBakePass: ", m_surfels.size(), " surfels captured.");

	return true;
}

bool GIBakePass::drawCubemaps(unsigned int probeIndex, const mat4& p, const std::vector<mat4>& v)
{
	auto l_renderingConfig = g_pModuleManager->getRenderingFrontend()->getRenderingConfig();

	drawOpaquePass(probeIndex, p, v);

	drawSkyVisibilityPass(probeIndex, p, v);

	return true;
}

bool GIBakePass::drawOpaquePass(unsigned int probeIndex, const mat4& p, const std::vector<mat4>& v)
{
	auto l_t = InnoMath::getInvertTranslationMatrix(m_probes[probeIndex].pos);

	std::vector<mat4> l_GICameraGPUData(8);
	l_GICameraGPUData[0] = p;
	for (size_t i = 0; i < 6; i++)
	{
		l_GICameraGPUData[i + 1] = v[i];
	}
	l_GICameraGPUData[7] = l_t;

	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(GetGPUBufferDataComponent(GPUBufferUsageType::GICamera), l_GICameraGPUData);

	auto l_MeshGBDC = GetGPUBufferDataComponent(GPUBufferUsageType::Mesh);
	auto l_MaterialGBDC = GetGPUBufferDataComponent(GPUBufferUsageType::Material);

	g_pModuleManager->getRenderingServer()->CommandListBegin(m_RPDC_Surfel, 0);
	g_pModuleManager->getRenderingServer()->BindRenderPassDataComponent(m_RPDC_Surfel);
	g_pModuleManager->getRenderingServer()->CleanRenderTargets(m_RPDC_Surfel);
	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, m_SDC_Surfel->m_ResourceBinder, 8, 0);
	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Surfel, ShaderStage::Geometry, GetGPUBufferDataComponent(GPUBufferUsageType::GICamera)->m_ResourceBinder, 0, 10, Accessibility::ReadOnly);

	unsigned int l_offset = 0;

	auto l_totalDrawCallCount = g_pModuleManager->getRenderingFrontend()->getGIPassDrawCallCount();
	for (unsigned int i = 0; i < l_totalDrawCallCount; i++)
	{
		auto l_GIPassGPUData = g_pModuleManager->getRenderingFrontend()->getGIPassGPUData()[i];

		if (l_GIPassGPUData.mesh->m_objectStatus == ObjectStatus::Activated)
		{
			g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Surfel, ShaderStage::Vertex, l_MeshGBDC->m_ResourceBinder, 1, 1, Accessibility::ReadOnly, l_offset, 1);
			g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_MaterialGBDC->m_ResourceBinder, 2, 2, Accessibility::ReadOnly, l_offset, 1);

			if (l_GIPassGPUData.material->m_objectStatus == ObjectStatus::Activated)
			{
				g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_GIPassGPUData.material->m_ResourceBinders[0], 3, 0);
				g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_GIPassGPUData.material->m_ResourceBinders[1], 4, 1);
				g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_GIPassGPUData.material->m_ResourceBinders[2], 5, 2);
				g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_GIPassGPUData.material->m_ResourceBinders[3], 6, 3);
				g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_GIPassGPUData.material->m_ResourceBinders[4], 7, 4);
			}

			g_pModuleManager->getRenderingServer()->DispatchDrawCall(m_RPDC_Surfel, l_GIPassGPUData.mesh);

			if (l_GIPassGPUData.material->m_objectStatus == ObjectStatus::Activated)
			{
				g_pModuleManager->getRenderingServer()->DeactivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_GIPassGPUData.material->m_ResourceBinders[0], 3, 0);
				g_pModuleManager->getRenderingServer()->DeactivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_GIPassGPUData.material->m_ResourceBinders[1], 4, 1);
				g_pModuleManager->getRenderingServer()->DeactivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_GIPassGPUData.material->m_ResourceBinders[2], 5, 2);
				g_pModuleManager->getRenderingServer()->DeactivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_GIPassGPUData.material->m_ResourceBinders[3], 6, 3);
				g_pModuleManager->getRenderingServer()->DeactivateResourceBinder(m_RPDC_Surfel, ShaderStage::Pixel, l_GIPassGPUData.material->m_ResourceBinders[4], 7, 4);
			}
		}

		l_offset++;
	}

	g_pModuleManager->getRenderingServer()->CommandListEnd(m_RPDC_Surfel);

	g_pModuleManager->getRenderingServer()->ExecuteCommandList(m_RPDC_Surfel);

	g_pModuleManager->getRenderingServer()->WaitForFrame(m_RPDC_Surfel);

	return true;
}

bool GIBakePass::drawSkyVisibilityPass(unsigned int probeIndex, const mat4& p, const std::vector<mat4>& v)
{
	auto l_depthStencilRT = g_pModuleManager->getRenderingServer()->ReadTextureBackToCPU(m_RPDC_Surfel, m_RPDC_Surfel->m_DepthStencilRenderTarget);

	auto l_depthStencilRTSize = l_depthStencilRT.size();

	l_depthStencilRTSize /= 6;

	for (size_t i = 0; i < 6; i++)
	{
		unsigned int l_stencil = 0;
		for (size_t j = 0; j < l_depthStencilRTSize; j++)
		{
			auto& l_depthStencil = l_depthStencilRT[i * l_depthStencilRTSize + j];

			if (l_depthStencil.w == 1.0f)
			{
				l_stencil++;
			}
		}

		m_probes[probeIndex].skyVisibility[i] = (float)l_stencil / (float)l_depthStencilRTSize;
	}

	return true;
}

bool GIBakePass::readBackSurfelCaches(unsigned int probeIndex)
{
	auto l_posWSMetallic = g_pModuleManager->getRenderingServer()->ReadTextureBackToCPU(m_RPDC_Surfel, m_RPDC_Surfel->m_RenderTargets[0]);
	auto l_normalRoughness = g_pModuleManager->getRenderingServer()->ReadTextureBackToCPU(m_RPDC_Surfel, m_RPDC_Surfel->m_RenderTargets[1]);
	auto l_albedoAO = g_pModuleManager->getRenderingServer()->ReadTextureBackToCPU(m_RPDC_Surfel, m_RPDC_Surfel->m_RenderTargets[2]);

	auto l_surfelSize = l_posWSMetallic.size();

	std::vector<Surfel> l_surfels(l_surfelSize);
	for (size_t i = 0; i < l_surfelSize; i++)
	{
		l_surfels[i].pos = l_posWSMetallic[i];
		l_surfels[i].pos.w = 1.0f;
		l_surfels[i].normal = l_normalRoughness[i];
		l_surfels[i].normal.w = 0.0f;
		l_surfels[i].albedo = l_albedoAO[i];
		l_surfels[i].albedo.w = 1.0f;
		l_surfels[i].MRAT.x = l_posWSMetallic[i].w;
		l_surfels[i].MRAT.y = l_normalRoughness[i].w;
		l_surfels[i].MRAT.z = l_albedoAO[i].w;
		l_surfels[i].MRAT.w = 1.0f;
	}

	m_surfels.insert(m_surfels.end(), l_surfels.begin(), l_surfels.end());

	return true;
}

bool GIBakePass::eliminateDuplicatedSurfels()
{
	g_pModuleManager->getLogSystem()->Log(LogLevel::Verbose, "GIBakePass: Eliminate duplicated surfels...");

	std::sort(m_surfels.begin(), m_surfels.end(), [&](Surfel A, Surfel B)
	{
		if (A.pos.x != B.pos.x) {
			return A.pos.x < B.pos.x;
		}
		if (A.pos.y != B.pos.y) {
			return A.pos.y < B.pos.y;
		}
		return A.pos.z < B.pos.z;
	});

	m_surfels.erase(std::unique(m_surfels.begin(), m_surfels.end()), m_surfels.end());
	m_surfels.shrink_to_fit();

	g_pModuleManager->getLogSystem()->Log(LogLevel::Success, "GIBakePass: Duplicated surfels have been removed, there are ", m_surfels.size(), " surfels now.");

	return true;
}

bool GIBakePass::generateBricks()
{
	g_pModuleManager->getLogSystem()->Log(LogLevel::Verbose, "GIBakePass: Generate brick caches...");

	// Find bound corner position
	auto l_surfelsCount = m_surfels.size();

	auto l_startPos = InnoMath::maxVec4<float>;
	l_startPos.w = 1.0f;

	auto l_endPos = InnoMath::minVec4<float>;
	l_endPos.w = 1.0f;

	for (size_t i = 0; i < l_surfelsCount; i++)
	{
		l_startPos = InnoMath::elementWiseMin(m_surfels[i].pos, l_startPos);
		l_endPos = InnoMath::elementWiseMax(m_surfels[i].pos, l_endPos);
	}

	// Fit the end corner to contain at least one brick in each axis
	auto l_adjustedEndPos = l_endPos;
	l_adjustedEndPos.x = (std::trunc(l_endPos.x / m_brickSize.x) + 1) * m_brickSize.x;
	l_adjustedEndPos.y = (std::trunc(l_endPos.y / m_brickSize.y) + 1) * m_brickSize.y;
	l_adjustedEndPos.z = (std::trunc(l_endPos.z / m_brickSize.z) + 1) * m_brickSize.z;
	l_endPos = l_adjustedEndPos;

	auto l_extends = l_endPos - l_startPos;
	auto l_bricksCountX = std::trunc(l_extends.x / m_brickSize.x);
	auto l_bricksCountY = std::trunc(l_extends.y / m_brickSize.y);
	auto l_bricksCountZ = std::trunc(l_extends.z / m_brickSize.z);

	auto l_totalBricksWorkCount = l_bricksCountX * l_bricksCountY * l_bricksCountZ;
	unsigned int l_index = 0;

	// Assign surfels to brick cache
	vec4 l_currentMaxPos = l_startPos + m_brickSize;
	vec4 l_currentMinPos = l_startPos;

	while (l_currentMaxPos.x <= l_endPos.x)
	{
		l_currentMaxPos.y = l_startPos.y + m_brickSize.y;
		l_currentMinPos.y = l_startPos.y;

		while (l_currentMaxPos.y <= l_endPos.y)
		{
			l_currentMaxPos.z = l_startPos.z + m_brickSize.z;
			l_currentMinPos.z = l_startPos.z;

			while (l_currentMaxPos.z <= l_endPos.z)
			{
				BrickCache l_BrickCache;
				l_BrickCache.pos = l_currentMinPos + m_brickSize / 2.0f;
				l_BrickCache.surfelCaches.reserve(l_surfelsCount);

				for (size_t i = 0; i < l_surfelsCount; i++)
				{
					if (
						InnoMath::isALessEqualThanBVec3(m_surfels[i].pos, l_currentMaxPos)
						&& InnoMath::isAGreaterEqualThanBVec3(m_surfels[i].pos, l_currentMinPos)
						)
					{
						l_BrickCache.surfelCaches.emplace_back(m_surfels[i]);
					}
				}

				l_BrickCache.surfelCaches.shrink_to_fit();

				if (l_BrickCache.surfelCaches.size() > 0)
				{
					m_brickCaches.emplace_back(std::move(l_BrickCache));
				}

				g_pModuleManager->getLogSystem()->Log(LogLevel::Verbose, "GIBakePass: Generate brick caches ", (float)l_index * 100.0f / (float)l_totalBricksWorkCount, "%...");

				l_index++;
				l_currentMaxPos.z += m_brickSize.z;
				l_currentMinPos.z += m_brickSize.z;
			}

			l_currentMaxPos.y += m_brickSize.y;
			l_currentMinPos.y += m_brickSize.y;
		}

		l_currentMaxPos.x += m_brickSize.x;
		l_currentMinPos.x += m_brickSize.x;
	}

	g_pModuleManager->getLogSystem()->Log(LogLevel::Success, "GIBakePass: Brick caches have been generated.");

	g_pModuleManager->getLogSystem()->Log(LogLevel::Verbose, "GIBakePass: Generate bricks...");

	// Generate real bricks with surfel range
	auto l_bricksCount = m_brickCaches.size();

	m_surfels.clear();

	size_t l_offset = 0;

	for (size_t i = 0; i < l_bricksCount; i++)
	{
		Brick l_brick;
		l_brick.boundBox = InnoMath::generateAABB(m_brickCaches[i].pos + m_brickSize / 2.0f, m_brickCaches[i].pos - m_brickSize / 2.0f);
		l_brick.surfelRangeBegin = (unsigned int)l_offset;
		l_brick.surfelRangeEnd = (unsigned int)(l_offset + m_brickCaches[i].surfelCaches.size() - 1);
		l_offset = m_brickCaches[i].surfelCaches.size();

		m_surfels.insert(m_surfels.end(), std::make_move_iterator(m_brickCaches[i].surfelCaches.begin()), std::make_move_iterator(m_brickCaches[i].surfelCaches.end()));

		m_bricks.emplace_back(l_brick);

		g_pModuleManager->getLogSystem()->Log(LogLevel::Verbose, "GIBakePass: Generate bricks ", (float)i * 100.0f / (float)l_bricksCount, "%...");
	}

	g_pModuleManager->getLogSystem()->Log(LogLevel::Success, "GIBakePass: Bricks have been generated.");

	return true;
}

bool GIBakePass::assignBrickFactorToProbesByGPU()
{
	auto l_rPX = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 0.0f));
	auto l_rNX = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(-1.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 0.0f));
	auto l_rPY = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, 1.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 0.0f));
	auto l_rNY = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 0.0f));
	auto l_rPZ = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 0.0f));
	auto l_rNZ = InnoMath::lookAt(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, -1.0f, 1.0f), vec4(0.0f, -1.0f, 0.0f, 0.0f));

	std::vector<mat4> l_v =
	{
		l_rPX, l_rNX, l_rPY, l_rNY, l_rPZ, l_rNZ
	};

	auto l_cameraGPUData = g_pModuleManager->getRenderingFrontend()->getCameraGPUData();

	auto l_p = InnoMath::generatePerspectiveMatrix((90.0f / 180.0f) * PI<float>, 1.0f, l_cameraGPUData.zNear, l_cameraGPUData.zFar);

	auto l_probesCount = m_probes.size();

	auto l_MeshGBDC = GetGPUBufferDataComponent(GPUBufferUsageType::Mesh);
	auto l_bricksCount = m_bricks.size();

	std::vector<MeshGPUData> l_bricksCubeMeshGPUData;
	l_bricksCubeMeshGPUData.resize(l_bricksCount);

	for (size_t i = 0; i < l_bricksCount; i++)
	{
		auto l_t = InnoMath::toTranslationMatrix(m_bricks[i].boundBox.m_center);
		auto l_s = InnoMath::toScaleMatrix(vec4(m_brickSize.x, m_brickSize.y, m_brickSize.z, 1.0f));

		l_bricksCubeMeshGPUData[i].m = l_t * l_s;

		// Index start from 1
		l_bricksCubeMeshGPUData[i].UUID = (float)i + 1.0f;
	}

	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(l_MeshGBDC, l_bricksCubeMeshGPUData, 0, l_bricksCubeMeshGPUData.size());

	for (size_t i = 0; i < l_probesCount; i++)
	{
		drawBricks((unsigned int)i, l_p, l_v);
		readBackBrickFactors((unsigned int)i);

		g_pModuleManager->getLogSystem()->Log(LogLevel::Verbose, "GIBakePass: Assign brick factor to probes ", (float)i * 100.0f / (float)l_probesCount, "%...");
	}

	g_pModuleManager->getLogSystem()->Log(LogLevel::Success, "GIBakePass: Brick factors have been generated.");

	return true;
}

bool GIBakePass::drawBricks(unsigned int probeIndex, const mat4 & p, const std::vector<mat4>& v)
{
	std::vector<mat4> l_GICameraGPUData(8);
	l_GICameraGPUData[0] = p;
	for (size_t i = 0; i < 6; i++)
	{
		l_GICameraGPUData[i + 1] = v[i];
	}
	l_GICameraGPUData[7] = InnoMath::getInvertTranslationMatrix(m_probes[probeIndex].pos);

	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(GetGPUBufferDataComponent(GPUBufferUsageType::GICamera), l_GICameraGPUData);

	auto l_MeshGBDC = GetGPUBufferDataComponent(GPUBufferUsageType::Mesh);

	auto l_mesh = g_pModuleManager->getRenderingFrontend()->getMeshDataComponent(MeshShapeType::Cube);

	unsigned int l_offset = 0;

	auto l_totalDrawCallCount = m_bricks.size();

	g_pModuleManager->getRenderingServer()->CommandListBegin(m_RPDC_BrickFactor, 0);
	g_pModuleManager->getRenderingServer()->BindRenderPassDataComponent(m_RPDC_BrickFactor);
	g_pModuleManager->getRenderingServer()->CleanRenderTargets(m_RPDC_BrickFactor);
	g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_BrickFactor, ShaderStage::Vertex, GetGPUBufferDataComponent(GPUBufferUsageType::GICamera)->m_ResourceBinder, 0, 10, Accessibility::ReadOnly);

	for (unsigned int i = 0; i < l_totalDrawCallCount; i++)
	{
		g_pModuleManager->getRenderingServer()->ActivateResourceBinder(m_RPDC_BrickFactor, ShaderStage::Vertex, l_MeshGBDC->m_ResourceBinder, 1, 1, Accessibility::ReadOnly, l_offset, 1);

		g_pModuleManager->getRenderingServer()->DispatchDrawCall(m_RPDC_BrickFactor, l_mesh);

		l_offset++;
	}

	g_pModuleManager->getRenderingServer()->CommandListEnd(m_RPDC_BrickFactor);

	g_pModuleManager->getRenderingServer()->ExecuteCommandList(m_RPDC_BrickFactor);

	g_pModuleManager->getRenderingServer()->WaitForFrame(m_RPDC_BrickFactor);

	return true;
}

bool GIBakePass::readBackBrickFactors(unsigned int probeIndex)
{
	auto l_brickIDResults = g_pModuleManager->getRenderingServer()->ReadTextureBackToCPU(m_RPDC_BrickFactor, m_RPDC_BrickFactor->m_RenderTargets[0]);

	auto l_brickIDResultSize = l_brickIDResults.size();

	l_brickIDResultSize /= 6;

	// 6 axis-aligned coefficients
	for (size_t i = 0; i < 6; i++)
	{
		std::vector<BrickFactor> l_brickFactors;
		l_brickFactors.reserve(l_brickIDResultSize);

		for (size_t j = 0; j < l_brickIDResultSize; j++)
		{
			auto& l_brickIDResult = l_brickIDResults[i * l_brickIDResultSize + j];
			if (l_brickIDResult.y != 0.0f)
			{
				BrickFactor l_BrickFactor;

				l_BrickFactor.basisWeight = std::abs(l_brickIDResult.x);

				// Index start from 1
				l_BrickFactor.brickIndex = (unsigned int)(std::round(l_brickIDResult.y) - 1.0f);
				l_brickFactors.emplace_back(l_BrickFactor);
			}
		}

		std::sort(l_brickFactors.begin(), l_brickFactors.end(), [&](BrickFactor A, BrickFactor B)
		{
			return A.brickIndex < B.brickIndex;
		});

		l_brickFactors.erase(std::unique(l_brickFactors.begin(), l_brickFactors.end()), l_brickFactors.end());
		l_brickFactors.shrink_to_fit();

		// Calculate brick weight
		auto l_brickFactorSize = l_brickFactors.size();

		if (l_brickFactorSize == 1)
		{
			l_brickFactors[0].basisWeight = 1.0f;
		}
		else
		{
			float denom = 0.0f;
			for (size_t i = 0; i < l_brickFactorSize; i++)
			{
				denom += l_brickFactors[i].basisWeight;
			}

			for (size_t i = 0; i < l_brickFactorSize; i++)
			{
				// Reverse view space Z axis
				l_brickFactors[i].basisWeight = (denom - l_brickFactors[i].basisWeight) / denom;
			}
		}

		// Assign brick factor range to probes
		auto l_brickFactorRangeBegin = m_brickFactors.size();
		auto l_brickFactorRangeEnd = l_brickFactorRangeBegin + l_brickFactors.size() - 1;

		m_probes[probeIndex].brickFactorRange[i * 2] = (unsigned int)l_brickFactorRangeBegin;
		m_probes[probeIndex].brickFactorRange[i * 2 + 1] = (unsigned int)l_brickFactorRangeEnd;

		m_brickFactors.insert(m_brickFactors.end(), std::make_move_iterator(l_brickFactors.begin()), std::make_move_iterator(l_brickFactors.end()));
	}

	return true;
}

bool GIBakePass::assignBrickFactorToProbesByCPU()
{
	auto l_probesCount = m_probes.size();
	auto l_bricksCount = m_bricks.size();

	std::vector<vec4> l_directions =
	{
		vec4(1.0f, 0.0f, 0.0f, 0.0f),
		vec4(-1.0f, 0.0f, 0.0f, 0.0f),
		vec4(0.0f, 1.0f, 0.0f, 0.0f),
		vec4(0.0f, -1.0f, 0.0f, 0.0f),
		vec4(0.0f, 0.0f, 1.0f, 0.0f),
		vec4(0.0f, 0.0f, -1.0f, 0.0f)
	};

	for (size_t i = 0; i < l_probesCount; i++)
	{
		std::vector<BrickFactor> l_brickFactors;
		l_brickFactors.reserve(l_bricksCount);

		Ray l_ray;
		l_ray.m_origin = m_probes[i].pos;

		for (size_t j = 0; j < 6; j++)
		{
			l_ray.m_direction = l_directions[j];

			for (size_t k = 0; k < l_bricksCount; k++)
			{
				if (InnoMath::intersectCheck(m_bricks[k].boundBox, l_ray))
				{
					BrickFactor l_BrickFactor;

					l_BrickFactor.basisWeight = (m_bricks[k].boundBox.m_center - l_ray.m_origin).length();
					l_BrickFactor.brickIndex = (unsigned int)k;

					l_brickFactors.emplace_back(l_BrickFactor);
				}
			}

			l_brickFactors.shrink_to_fit();

			// Calculate brick weight
			auto l_brickFactorSize = l_brickFactors.size();

			if (l_brickFactorSize == 1)
			{
				l_brickFactors[0].basisWeight = 1.0f;
			}
			else
			{
				float denom = 0.0f;
				for (size_t i = 0; i < l_brickFactorSize; i++)
				{
					denom += l_brickFactors[i].basisWeight;
				}

				for (size_t i = 0; i < l_brickFactorSize; i++)
				{
					l_brickFactors[i].basisWeight = (l_brickFactors[i].basisWeight) / denom;
				}
			}

			// Assign brick factor range to probes
			auto l_brickFactorRangeBegin = m_brickFactors.size();
			auto l_brickFactorRangeEnd = l_brickFactorRangeBegin + l_brickFactors.size() - 1;

			m_probes[i].brickFactorRange[j * 2] = (unsigned int)l_brickFactorRangeBegin;
			m_probes[i].brickFactorRange[j * 2 + 1] = (unsigned int)l_brickFactorRangeEnd;

			m_brickFactors.insert(m_brickFactors.end(), std::make_move_iterator(l_brickFactors.begin()), std::make_move_iterator(l_brickFactors.end()));
		}

		g_pModuleManager->getLogSystem()->Log(LogLevel::Verbose, "GIBakePass: Assign brick factor to probes ", (float)i * 100.0f / (float)l_probesCount, "%...");
	}

	g_pModuleManager->getLogSystem()->Log(LogLevel::Success, "GIBakePass: Brick factors have been generated.");

	return true;
}

bool GIBakePass::serializeSurfels()
{
	auto l_filePath = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_currentSceneName = g_pModuleManager->getFileSystem()->getCurrentSceneName();

	std::ofstream l_file;
	l_file.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoSurfel", std::ios::binary);
	IOService::serializeVector(l_file, m_surfels);

	return true;
}

bool GIBakePass::serializeBricks()
{
	auto l_filePath = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_currentSceneName = g_pModuleManager->getFileSystem()->getCurrentSceneName();

	std::ofstream l_file;
	l_file.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoBrick", std::ios::binary);
	IOService::serializeVector(l_file, m_bricks);

	return true;
}

bool GIBakePass::serializeBrickFactors()
{
	auto l_filePath = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_currentSceneName = g_pModuleManager->getFileSystem()->getCurrentSceneName();

	std::ofstream l_file;
	l_file.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoBrickFactor", std::ios::binary);
	IOService::serializeVector(l_file, m_brickFactors);

	return true;
}

bool GIBakePass::serializeProbes()
{
	auto l_filePath = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_currentSceneName = g_pModuleManager->getFileSystem()->getCurrentSceneName();

	std::ofstream l_file;
	l_file.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoProbe", std::ios::binary);
	IOService::serializeVector(l_file, m_probes);

	return true;
}

bool GIBakePass::Setup()
{
	f_sceneLoadingFinishCallback = []()
	{
		m_IsSurfelLoaded = false;
		m_IsBrickLoaded = false;
		m_IsBrickFactorLoaded = false;
		m_IsProbeLoaded = false;
		loadGIData();
	};

	g_pModuleManager->getFileSystem()->addSceneLoadingFinishCallback(&f_sceneLoadingFinishCallback);

	////
	m_testSampleCubemap = g_pModuleManager->getRenderingServer()->AddTextureDataComponent("TestSampleCubemap/");

	std::vector<vec4> l_cubemapTextureSamples(m_captureResolution * m_captureResolution * 6);
	std::vector<vec4> l_faceColors = {
	vec4(1.0f, 0.0f, 0.0f, 1.0f),
	vec4(1.0f, 1.0f, 0.0f, 1.0f),
	vec4(0.0f, 1.0f, 0.0f, 1.0f),
	vec4(0.0f, 1.0f, 1.0f, 1.0f),
	vec4(0.0f, 0.0f, 1.0f, 1.0f),
	vec4(1.0f, 0.0f, 1.0f, 1.0f),
	};
	for (size_t i = 0; i < 6; i++)
	{
		for (size_t j = 0; j < m_sampleCountPerFace; j++)
		{
			auto l_color = l_faceColors[i] * 2.0f * (float)j / (float)m_sampleCountPerFace;
			l_color.w = 1.0f;
			l_cubemapTextureSamples[i * m_sampleCountPerFace + j] = l_color;
		}
	}

	auto l_RenderPassDesc = g_pModuleManager->getRenderingFrontend()->getDefaultRenderPassDesc();

	m_testSampleCubemap->m_textureDataDesc = l_RenderPassDesc.m_RenderTargetDesc;
	m_testSampleCubemap->m_textureDataDesc.SamplerType = TextureSamplerType::SamplerCubemap;
	m_testSampleCubemap->m_textureDataDesc.UsageType = TextureUsageType::Normal;
	m_testSampleCubemap->m_textureDataDesc.PixelDataFormat = TexturePixelDataFormat::RGBA;
	m_testSampleCubemap->m_textureDataDesc.MinFilterMethod = TextureFilterMethod::Linear;
	m_testSampleCubemap->m_textureDataDesc.MagFilterMethod = TextureFilterMethod::Linear;
	m_testSampleCubemap->m_textureDataDesc.WrapMethod = TextureWrapMethod::Repeat;
	m_testSampleCubemap->m_textureDataDesc.Width = m_captureResolution;
	m_testSampleCubemap->m_textureDataDesc.Height = m_captureResolution;
	m_testSampleCubemap->m_textureDataDesc.PixelDataType = TexturePixelDataType::FLOAT32;
	m_testSampleCubemap->m_textureData = &l_cubemapTextureSamples[0];

	g_pModuleManager->getRenderingServer()->InitializeTextureDataComponent(m_testSampleCubemap);

	////
	std::vector<vec4> l_3DTextureSamples(m_captureResolution * m_captureResolution * m_captureResolution);
	size_t l_pixelIndex = 0;
	for (size_t i = 0; i < m_captureResolution; i++)
	{
		for (size_t j = 0; j < m_captureResolution; j++)
		{
			for (size_t k = 0; k < m_captureResolution; k++)
			{
				l_3DTextureSamples[l_pixelIndex] = vec4((float)i / (float)m_captureResolution, (float)j / (float)m_captureResolution, (float)k / (float)m_captureResolution, 1.0f);
				l_pixelIndex++;
			}
		}
	}

	m_testSample3DTexture = g_pModuleManager->getRenderingServer()->AddTextureDataComponent("TestSample3D/");

	m_testSample3DTexture->m_textureDataDesc = l_RenderPassDesc.m_RenderTargetDesc;
	m_testSample3DTexture->m_textureDataDesc.SamplerType = TextureSamplerType::Sampler3D;
	m_testSample3DTexture->m_textureDataDesc.UsageType = TextureUsageType::Normal;
	m_testSample3DTexture->m_textureDataDesc.PixelDataFormat = TexturePixelDataFormat::RGBA;
	m_testSample3DTexture->m_textureDataDesc.MinFilterMethod = TextureFilterMethod::Linear;
	m_testSample3DTexture->m_textureDataDesc.MagFilterMethod = TextureFilterMethod::Linear;
	m_testSample3DTexture->m_textureDataDesc.WrapMethod = TextureWrapMethod::Repeat;
	m_testSample3DTexture->m_textureDataDesc.Width = m_captureResolution;
	m_testSample3DTexture->m_textureDataDesc.Height = m_captureResolution;
	m_testSample3DTexture->m_textureDataDesc.DepthOrArraySize = m_captureResolution;
	m_testSample3DTexture->m_textureDataDesc.PixelDataType = TexturePixelDataType::FLOAT32;
	m_testSample3DTexture->m_textureData = &l_3DTextureSamples[0];

	g_pModuleManager->getRenderingServer()->InitializeTextureDataComponent(m_testSample3DTexture);

	////
	m_SPC_Probe = g_pModuleManager->getRenderingServer()->AddShaderProgramComponent("GIBakeProbePass/");

	m_SPC_Probe->m_ShaderFilePaths.m_VSPath = "GIBakeProbePass.vert/";
	m_SPC_Probe->m_ShaderFilePaths.m_PSPath = "GIBakeProbePass.frag/";

	g_pModuleManager->getRenderingServer()->InitializeShaderProgramComponent(m_SPC_Probe);

	m_RPDC_Probe = g_pModuleManager->getRenderingServer()->AddRenderPassDataComponent("GIBakeProbePass/");

	m_RPDC_Probe->m_RenderPassDesc = l_RenderPassDesc;
	m_RPDC_Probe->m_RenderPassDesc.m_RenderTargetDesc.SamplerType = TextureSamplerType::Sampler2D;
	m_RPDC_Probe->m_RenderPassDesc.m_RenderTargetDesc.Width = m_probeMapResolution;
	m_RPDC_Probe->m_RenderPassDesc.m_RenderTargetDesc.Height = m_probeMapResolution;
	m_RPDC_Probe->m_RenderPassDesc.m_RenderTargetDesc.PixelDataType = TexturePixelDataType::FLOAT32;

	m_RPDC_Probe->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_UseDepthBuffer = true;
	m_RPDC_Probe->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_AllowDepthWrite = true;
	m_RPDC_Probe->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_DepthComparisionFunction = ComparisionFunction::LessEqual;

	m_RPDC_Probe->m_RenderPassDesc.m_GraphicsPipelineDesc.m_RasterizerDesc.m_UseCulling = true;
	m_RPDC_Probe->m_RenderPassDesc.m_GraphicsPipelineDesc.m_ViewportDesc.m_Width = m_probeMapResolution;
	m_RPDC_Probe->m_RenderPassDesc.m_GraphicsPipelineDesc.m_ViewportDesc.m_Height = m_probeMapResolution;

	m_RPDC_Probe->m_ResourceBinderLayoutDescs.resize(2);
	m_RPDC_Probe->m_ResourceBinderLayoutDescs[0].m_ResourceBinderType = ResourceBinderType::Buffer;
	m_RPDC_Probe->m_ResourceBinderLayoutDescs[0].m_GlobalSlot = 0;
	m_RPDC_Probe->m_ResourceBinderLayoutDescs[0].m_LocalSlot = 10;

	m_RPDC_Probe->m_ResourceBinderLayoutDescs[1].m_ResourceBinderType = ResourceBinderType::Buffer;
	m_RPDC_Probe->m_ResourceBinderLayoutDescs[1].m_GlobalSlot = 1;
	m_RPDC_Probe->m_ResourceBinderLayoutDescs[1].m_LocalSlot = 1;

	m_RPDC_Probe->m_ShaderProgram = m_SPC_Probe;

	g_pModuleManager->getRenderingServer()->InitializeRenderPassDataComponent(m_RPDC_Probe);

	////
	m_SPC_Surfel = g_pModuleManager->getRenderingServer()->AddShaderProgramComponent("GIBakeSurfelPass/");

	m_SPC_Surfel->m_ShaderFilePaths.m_VSPath = "GIBakeSurfelPass.vert/";
	m_SPC_Surfel->m_ShaderFilePaths.m_GSPath = "GIBakeSurfelPass.geom/";
	m_SPC_Surfel->m_ShaderFilePaths.m_PSPath = "GIBakeSurfelPass.frag/";

	g_pModuleManager->getRenderingServer()->InitializeShaderProgramComponent(m_SPC_Surfel);

	m_RPDC_Surfel = g_pModuleManager->getRenderingServer()->AddRenderPassDataComponent("GIBakeSurfelPass/");

	m_RPDC_Surfel->m_RenderPassDesc = l_RenderPassDesc;

	m_RPDC_Surfel->m_RenderPassDesc.m_RenderTargetCount = 3;

	m_RPDC_Surfel->m_RenderPassDesc.m_RenderTargetDesc.SamplerType = TextureSamplerType::SamplerCubemap;
	m_RPDC_Surfel->m_RenderPassDesc.m_RenderTargetDesc.Width = m_captureResolution;
	m_RPDC_Surfel->m_RenderPassDesc.m_RenderTargetDesc.Height = m_captureResolution;
	m_RPDC_Surfel->m_RenderPassDesc.m_RenderTargetDesc.PixelDataType = TexturePixelDataType::FLOAT32;

	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_UseDepthBuffer = true;
	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_AllowDepthWrite = true;
	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_DepthComparisionFunction = ComparisionFunction::LessEqual;

	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_UseStencilBuffer = true;
	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_AllowStencilWrite = true;
	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_StencilReference = 0x01;
	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_FrontFaceStencilPassOperation = StencilOperation::Replace;
	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_FrontFaceStencilComparisionFunction = ComparisionFunction::Always;
	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_BackFaceStencilPassOperation = StencilOperation::Replace;
	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_BackFaceStencilComparisionFunction = ComparisionFunction::Always;

	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_RasterizerDesc.m_UseCulling = true;
	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_ViewportDesc.m_Width = m_captureResolution;
	m_RPDC_Surfel->m_RenderPassDesc.m_GraphicsPipelineDesc.m_ViewportDesc.m_Height = m_captureResolution;

	m_RPDC_Surfel->m_ResourceBinderLayoutDescs.resize(9);
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[0].m_ResourceBinderType = ResourceBinderType::Buffer;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[0].m_GlobalSlot = 0;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[0].m_LocalSlot = 10;

	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[1].m_ResourceBinderType = ResourceBinderType::Buffer;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[1].m_GlobalSlot = 1;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[1].m_LocalSlot = 1;

	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[2].m_ResourceBinderType = ResourceBinderType::Buffer;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[2].m_GlobalSlot = 2;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[2].m_LocalSlot = 2;

	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[3].m_ResourceBinderType = ResourceBinderType::Image;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[3].m_GlobalSlot = 3;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[3].m_LocalSlot = 0;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[3].m_IsRanged = true;

	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[4].m_ResourceBinderType = ResourceBinderType::Image;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[4].m_GlobalSlot = 4;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[4].m_LocalSlot = 1;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[4].m_IsRanged = true;

	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[5].m_ResourceBinderType = ResourceBinderType::Image;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[5].m_GlobalSlot = 5;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[5].m_LocalSlot = 2;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[5].m_ResourceCount = 1;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[5].m_IsRanged = true;

	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[6].m_ResourceBinderType = ResourceBinderType::Image;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[6].m_GlobalSlot = 6;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[6].m_LocalSlot = 3;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[6].m_IsRanged = true;

	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[7].m_ResourceBinderType = ResourceBinderType::Image;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[7].m_GlobalSlot = 7;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[7].m_LocalSlot = 4;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[7].m_IsRanged = true;

	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[8].m_ResourceBinderType = ResourceBinderType::Sampler;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[8].m_GlobalSlot = 8;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[8].m_LocalSlot = 0;
	m_RPDC_Surfel->m_ResourceBinderLayoutDescs[8].m_IsRanged = true;

	m_RPDC_Surfel->m_ShaderProgram = m_SPC_Surfel;

	g_pModuleManager->getRenderingServer()->InitializeRenderPassDataComponent(m_RPDC_Surfel);

	m_SDC_Surfel = g_pModuleManager->getRenderingServer()->AddSamplerDataComponent("GIBakeSurfelPass/");

	m_SDC_Surfel->m_SamplerDesc.m_WrapMethodU = TextureWrapMethod::Repeat;
	m_SDC_Surfel->m_SamplerDesc.m_WrapMethodV = TextureWrapMethod::Repeat;

	g_pModuleManager->getRenderingServer()->InitializeSamplerDataComponent(m_SDC_Surfel);

	////
	m_SPC_BrickFactor = g_pModuleManager->getRenderingServer()->AddShaderProgramComponent("GIBakeBrickFactorPass/");

	m_SPC_BrickFactor->m_ShaderFilePaths.m_VSPath = "GIBakeBrickFactorPass.vert/";
	m_SPC_BrickFactor->m_ShaderFilePaths.m_GSPath = "GIBakeBrickFactorPass.geom/";
	m_SPC_BrickFactor->m_ShaderFilePaths.m_PSPath = "GIBakeBrickFactorPass.frag/";

	g_pModuleManager->getRenderingServer()->InitializeShaderProgramComponent(m_SPC_BrickFactor);

	m_RPDC_BrickFactor = g_pModuleManager->getRenderingServer()->AddRenderPassDataComponent("GIBakeBrickFactorPass/");

	m_RPDC_BrickFactor->m_RenderPassDesc = l_RenderPassDesc;
	m_RPDC_BrickFactor->m_RenderPassDesc.m_RenderTargetDesc.SamplerType = TextureSamplerType::SamplerCubemap;
	m_RPDC_BrickFactor->m_RenderPassDesc.m_RenderTargetDesc.Width = m_probeMapSamplingInterval;
	m_RPDC_BrickFactor->m_RenderPassDesc.m_RenderTargetDesc.Height = m_probeMapSamplingInterval;
	m_RPDC_BrickFactor->m_RenderPassDesc.m_RenderTargetDesc.PixelDataType = TexturePixelDataType::FLOAT32;

	m_RPDC_BrickFactor->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_UseDepthBuffer = true;
	m_RPDC_BrickFactor->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_AllowDepthWrite = true;
	m_RPDC_BrickFactor->m_RenderPassDesc.m_GraphicsPipelineDesc.m_DepthStencilDesc.m_DepthComparisionFunction = ComparisionFunction::LessEqual;

	m_RPDC_BrickFactor->m_RenderPassDesc.m_GraphicsPipelineDesc.m_RasterizerDesc.m_UseCulling = true;
	m_RPDC_BrickFactor->m_RenderPassDesc.m_GraphicsPipelineDesc.m_ViewportDesc.m_Width = m_probeMapSamplingInterval;
	m_RPDC_BrickFactor->m_RenderPassDesc.m_GraphicsPipelineDesc.m_ViewportDesc.m_Height = m_probeMapSamplingInterval;

	m_RPDC_BrickFactor->m_ResourceBinderLayoutDescs.resize(2);
	m_RPDC_BrickFactor->m_ResourceBinderLayoutDescs[0].m_ResourceBinderType = ResourceBinderType::Buffer;
	m_RPDC_BrickFactor->m_ResourceBinderLayoutDescs[0].m_GlobalSlot = 0;
	m_RPDC_BrickFactor->m_ResourceBinderLayoutDescs[0].m_LocalSlot = 10;

	m_RPDC_BrickFactor->m_ResourceBinderLayoutDescs[1].m_ResourceBinderType = ResourceBinderType::Buffer;
	m_RPDC_BrickFactor->m_ResourceBinderLayoutDescs[1].m_GlobalSlot = 1;
	m_RPDC_BrickFactor->m_ResourceBinderLayoutDescs[1].m_LocalSlot = 1;

	m_RPDC_BrickFactor->m_ShaderProgram = m_SPC_BrickFactor;

	g_pModuleManager->getRenderingServer()->InitializeRenderPassDataComponent(m_RPDC_BrickFactor);

	////
	m_testSurfelTexture = g_pModuleManager->getRenderingServer()->AddTextureDataComponent("TestSurfel/");
	m_testSurfelTexture->m_textureDataDesc = m_RPDC_Surfel->m_RenderPassDesc.m_RenderTargetDesc;
	m_testSurfelTexture->m_textureDataDesc.UsageType = TextureUsageType::Normal;

	return true;
}

bool GIBakePass::Initialize()
{
	return true;
}

bool GIBakePass::Bake()
{
	auto l_MeshGBDC = GetGPUBufferDataComponent(GPUBufferUsageType::Mesh);
	auto l_MaterialGBDC = GetGPUBufferDataComponent(GPUBufferUsageType::Material);

	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(l_MeshGBDC, g_pModuleManager->getRenderingFrontend()->getGIPassMeshGPUData());
	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(l_MaterialGBDC, g_pModuleManager->getRenderingFrontend()->getGIPassMaterialGPUData());

	m_probes.clear();
	m_surfels.clear();
	m_bricks.clear();
	m_brickCaches.clear();
	m_brickFactors.clear();

	generateProbes();

	captureSurfels();

	eliminateDuplicatedSurfels();

	generateBricks();

	serializeSurfels();
	serializeBricks();

	//assignBrickFactorToProbesByCPU();
	assignBrickFactorToProbesByGPU();

	serializeBrickFactors();
	serializeProbes();

	loadGIData();
	GIResolvePass::DeleteGPUBuffers();
	GIResolvePass::InitializeGPUBuffers();

	return true;
}

bool GIBakePass::PrepareCommandList()
{
	return true;
}

bool GIBakePass::ExecuteCommandList()
{
	return true;
}

bool GIBakePass::Terminate()
{
	g_pModuleManager->getRenderingServer()->DeleteRenderPassDataComponent(m_RPDC_Surfel);

	return true;
}

RenderPassDataComponent * GIBakePass::GetRPDC()
{
	return m_RPDC_Surfel;
}

ShaderProgramComponent * GIBakePass::GetSPC()
{
	return m_SPC_Surfel;
}

const std::vector<Surfel>& GIBakePass::GetSurfels()
{
	return m_surfels;
}

const std::vector<Brick>& GIBakePass::GetBricks()
{
	return m_bricks;
}

const std::vector<BrickFactor>& GIBakePass::GetBrickFactors()
{
	return m_brickFactors;
}

const std::vector<Probe>& GIBakePass::GetProbes()
{
	return m_probes;
}

vec4 GIBakePass::GetIrradianceVolumeRange()
{
	return m_irradianceVolumeRange;
}