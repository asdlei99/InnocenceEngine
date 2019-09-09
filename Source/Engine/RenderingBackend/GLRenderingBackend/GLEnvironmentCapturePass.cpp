#include "GLEnvironmentCapturePass.h"
#include "GLRenderingBackendUtilities.h"

#include "GLSkyPass.h"
#include "GLOpaquePass.h"
#include "GLSHPass.h"

#include "../../Component/GLRenderingBackendComponent.h"

#include "../../ModuleManager/IModuleManager.h"

extern IModuleManager* g_pModuleManager;

using namespace GLRenderingBackendNS;

#include "../../FileSystem/IOService.h"

struct ProbeCache
{
	vec4 pos;
	std::vector<Surfel> surfelCaches;
};

namespace GLEnvironmentCapturePass
{
	bool loadGIData();

	bool generateProbes();
	bool generateBricks();

	bool capture();
	bool drawCubemaps(uint32_t probeIndex, const mat4& p, const std::vector<mat4>& v);
	bool drawOpaquePass(uint32_t probeIndex, const mat4& p, const std::vector<mat4>& v, uint32_t faceIndex);
	bool readBackSurfelCaches(uint32_t probeIndex);
	bool drawSkyVisibilityPass(const mat4& p, const std::vector<mat4>& v, uint32_t faceIndex);
	bool drawSkyPass(const mat4& p, const std::vector<mat4>& v, uint32_t faceIndex);
	bool drawLightPass(const mat4& p, const std::vector<mat4>& v, uint32_t faceIndex);

	bool eliminateDuplicatedSurfels();
	bool eliminateEmptyBricks();
	bool isBrickEmpty(const std::vector<Surfel>& surfels, const Brick& brick);
	bool assignSurfelRangeToBricks();
	bool findSurfelRangeForBrick(Brick& brick);
	bool assignBrickFactorToProbes();

	bool serializeProbes();
	bool serializeSurfels();
	bool serializeBricks();
	bool serializeBrickFactors();

	EntityID m_EntityID;

	GLRenderPassComponent* m_opaquePassGLRPC;
	GLRenderPassComponent* m_capturePassGLRPC;
	GLRenderPassComponent* m_skyVisibilityPassGLRPC;

	GLShaderProgramComponent* m_capturePassGLSPC;
	ShaderFilePaths m_capturePassShaderFilePaths = { "environmentCapturePass.vert/" , "", "", "", "environmentCapturePass.frag/" };

	GLShaderProgramComponent* m_skyVisibilityGLSPC;
	ShaderFilePaths m_skyVisibilityShaderFilePaths = { "skyVisibilityPass.vert/" , "", "", "", "skyVisibilityPass.frag/" };

	const uint32_t m_captureResolution = 128;
	const uint32_t m_sampleCountPerFace = m_captureResolution * m_captureResolution;
	const uint32_t m_subDivideDimension = 2;
	const uint32_t m_totalCaptureProbes = m_subDivideDimension * m_subDivideDimension * m_subDivideDimension;

	std::vector<ProbeCache> m_probeCaches;
	std::vector<Probe> m_probes;
	std::vector<BrickFactor> m_brickFactors;
	std::vector<Brick> m_brickCaches;
	std::vector<Brick> m_bricks;
	std::vector<Surfel> m_surfels;
}

bool GLEnvironmentCapturePass::initialize()
{
	m_EntityID = InnoMath::createEntityID();

	auto l_renderPassDesc = GLRenderingBackendComponent::get().m_deferredRenderPassDesc;

	l_renderPassDesc.RTNumber = 4;
	l_renderPassDesc.RTDesc.samplerType = TextureSamplerType::SAMPLER_CUBEMAP;
	l_renderPassDesc.RTDesc.usageType = TextureUsageType::COLOR_ATTACHMENT;
	l_renderPassDesc.RTDesc.pixelDataFormat = TexturePixelDataFormat::RGBA;
	l_renderPassDesc.RTDesc.minFilterMethod = TextureFilterMethod::LINEAR;
	l_renderPassDesc.RTDesc.magFilterMethod = TextureFilterMethod::LINEAR;
	l_renderPassDesc.RTDesc.wrapMethod = TextureWrapMethod::REPEAT;
	l_renderPassDesc.RTDesc.width = m_captureResolution;
	l_renderPassDesc.RTDesc.height = m_captureResolution;
	l_renderPassDesc.RTDesc.pixelDataType = TexturePixelDataType::FLOAT32;
	l_renderPassDesc.useDepthAttachment = true;
	l_renderPassDesc.useStencilAttachment = true;

	m_opaquePassGLRPC = addGLRenderPassComponent(m_EntityID, "EnvironmentCaptureOpaquePassGLRPC/");
	m_opaquePassGLRPC->m_renderPassDesc = l_renderPassDesc;
	m_opaquePassGLRPC->m_drawColorBuffers = true;

	initializeGLRenderPassComponent(m_opaquePassGLRPC);

	m_capturePassGLRPC = addGLRenderPassComponent(m_EntityID, "EnvironmentCaptureLightPassGLRPC/");
	l_renderPassDesc.RTNumber = 1;
	m_capturePassGLRPC->m_renderPassDesc = l_renderPassDesc;
	m_capturePassGLRPC->m_drawColorBuffers = true;

	initializeGLRenderPassComponent(m_capturePassGLRPC);

	m_skyVisibilityPassGLRPC = addGLRenderPassComponent(m_EntityID, "EnvironmentCaptureSkyVisibilityPassGLRPC/");
	l_renderPassDesc.RTNumber = 1;
	l_renderPassDesc.RTDesc.pixelDataFormat = TexturePixelDataFormat::RGBA;
	l_renderPassDesc.RTDesc.pixelDataType = TexturePixelDataType::FLOAT32;
	m_skyVisibilityPassGLRPC->m_renderPassDesc = l_renderPassDesc;
	m_skyVisibilityPassGLRPC->m_drawColorBuffers = true;

	initializeGLRenderPassComponent(m_skyVisibilityPassGLRPC);

	m_capturePassGLSPC = addGLShaderProgramComponent(m_EntityID);
	initializeGLShaderProgramComponent(m_capturePassGLSPC, m_capturePassShaderFilePaths);

	m_skyVisibilityGLSPC = addGLShaderProgramComponent(m_EntityID);
	initializeGLShaderProgramComponent(m_skyVisibilityGLSPC, m_skyVisibilityShaderFilePaths);

	return true;
}

bool GLEnvironmentCapturePass::loadGIData()
{
	auto l_filePath = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_currentSceneName = g_pModuleManager->getFileSystem()->getCurrentSceneName();

	std::ifstream l_probeFile;
	l_probeFile.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoProbe", std::ios::binary);

	if (l_probeFile.is_open())
	{
		IOService::deserializeVector(l_probeFile, m_probes);

		std::ifstream l_surfelFile;
		l_surfelFile.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoSurfel", std::ios::binary);
		IOService::deserializeVector(l_surfelFile, m_surfels);

		std::ifstream l_brickFile;
		l_brickFile.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoBrick", std::ios::binary);
		IOService::deserializeVector(l_brickFile, m_bricks);

		std::ifstream l_brickFactorFile;
		l_brickFactorFile.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoBrickFactor", std::ios::binary);
		IOService::deserializeVector(l_brickFactorFile, m_brickFactors);

		return true;
	}
	else
	{
		return false;
	}
}

bool GLEnvironmentCapturePass::generateProbes()
{
	auto l_sceneAABB = g_pModuleManager->getPhysicsSystem()->getTotalSceneAABB();

	auto l_sceneCenter = l_sceneAABB.m_center;
	auto l_extendedAxisSize = l_sceneAABB.m_extend;
	l_extendedAxisSize = l_extendedAxisSize - vec4(32.0, 2.0f, 32.0f, 0.0f);
	l_extendedAxisSize.w = 0.0f;
	auto l_probeDistance = l_extendedAxisSize / (float)(m_subDivideDimension + 1);
	auto l_startPos = l_sceneAABB.m_center - (l_extendedAxisSize / 2.0f);
	l_startPos.x += l_probeDistance.x;
	l_startPos.z += l_probeDistance.z;
	auto l_currentPos = l_startPos;

	uint32_t l_probeIndex = 0;
	for (size_t i = 0; i < m_subDivideDimension; i++)
	{
		l_currentPos.y = l_startPos.y;
		for (size_t j = 0; j < m_subDivideDimension; j++)
		{
			l_currentPos.z = l_startPos.z;
			for (size_t k = 0; k < m_subDivideDimension; k++)
			{
				Probe l_probe;
				l_probe.pos = l_currentPos;
				m_probes.emplace_back(l_probe);

				ProbeCache l_probeCache;
				l_probeCache.pos = l_currentPos;
				m_probeCaches.emplace_back(l_probeCache);

				l_currentPos.z += l_probeDistance.z;
				l_probeIndex++;
			}
			l_currentPos.y += l_probeDistance.y;
		}
		l_currentPos.x += l_probeDistance.x;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_VERBOSE, "GLRenderingBackend: Generating probes: " + std::to_string((float)l_probeIndex * 100.0f / (float)m_totalCaptureProbes) + "%");
	}

	return true;
}

bool GLEnvironmentCapturePass::generateBricks()
{
	auto l_sceneAABB = g_pModuleManager->getPhysicsSystem()->getTotalSceneAABB();

	auto l_sceneCenter = l_sceneAABB.m_center;
	auto l_extendedAxisSize = l_sceneAABB.m_extend;
	l_extendedAxisSize.w = 0.0f;
	auto l_startPos = l_sceneAABB.m_center - (l_extendedAxisSize / 2.0f);
	auto l_currentPos = l_startPos;

	auto l_brickSize = 16.0f;
	auto l_maxBrickCountX = (uint32_t)std::ceil(l_extendedAxisSize.x / l_brickSize);
	auto l_maxBrickCountY = (uint32_t)std::ceil(l_extendedAxisSize.y / l_brickSize);
	auto l_maxBrickCountZ = (uint32_t)std::ceil(l_extendedAxisSize.z / l_brickSize);

	auto l_totalBricks = l_maxBrickCountX * l_maxBrickCountY * l_maxBrickCountZ;

	uint32_t l_brickIndex = 0;
	for (size_t i = 0; i < l_maxBrickCountX; i++)
	{
		l_currentPos.y = l_startPos.y;
		for (size_t j = 0; j < l_maxBrickCountY; j++)
		{
			l_currentPos.z = l_startPos.z;
			for (size_t k = 0; k < l_maxBrickCountZ; k++)
			{
				AABB l_brickAABB;
				l_brickAABB.m_boundMin = l_currentPos;
				l_brickAABB.m_extend = vec4(l_brickSize, l_brickSize, l_brickSize, 0.0f);
				l_brickAABB.m_boundMax = l_currentPos + l_brickAABB.m_extend;
				l_brickAABB.m_center = l_currentPos + (l_brickAABB.m_extend / 2.0f);

				Brick l_brick;
				l_brick.boundBox = l_brickAABB;
				m_brickCaches.emplace_back(l_brick);

				l_currentPos.z += l_brickSize;
				l_brickIndex++;
			}
			l_currentPos.y += l_brickSize;
		}
		l_currentPos.x += l_brickSize;
		g_pModuleManager->getLogSystem()->printLog(LogType::INNO_DEV_VERBOSE, "GLRenderingBackend: Generating brick: " + std::to_string((float)l_brickIndex * 100.0f / (float)l_totalBricks) + "%");
	}

	return true;
}

bool GLEnvironmentCapturePass::capture()
{
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

	for (uint32_t i = 0; i < m_totalCaptureProbes; i++)
	{
		drawCubemaps(i, l_p, l_v);
		readBackSurfelCaches(i);

		auto l_SH9 = GLSHPass::getSH9(m_capturePassGLRPC->m_GLTDCs[0]);
		m_probes[i].radiance = l_SH9;

		l_SH9 = GLSHPass::getSH9(m_skyVisibilityPassGLRPC->m_GLTDCs[0]);
		m_probes[i].skyVisibility = l_SH9;
	}

	return true;
}

bool GLEnvironmentCapturePass::drawCubemaps(uint32_t probeIndex, const mat4& p, const std::vector<mat4>& v)
{
	auto l_renderingConfig = g_pModuleManager->getRenderingFrontend()->getRenderingConfig();

	for (uint32_t i = 0; i < 6; i++)
	{
		drawOpaquePass(probeIndex, p, v, i);

		bindRenderPass(m_skyVisibilityPassGLRPC);
		cleanRenderBuffers(m_skyVisibilityPassGLRPC);

		drawSkyVisibilityPass(p, v, i);

		bindRenderPass(m_capturePassGLRPC);
		cleanRenderBuffers(m_capturePassGLRPC);

		if (l_renderingConfig.drawSky)
		{
			drawSkyPass(p, v, i);
		}

		drawLightPass(p, v, i);
	}

	return true;
}

bool GLEnvironmentCapturePass::drawOpaquePass(uint32_t probeIndex, const mat4& p, const std::vector<mat4>& v, uint32_t faceIndex)
{
	bindRenderPass(m_opaquePassGLRPC);
	cleanRenderBuffers(m_opaquePassGLRPC);

	CameraGPUData l_cameraGPUData;
	l_cameraGPUData.p_original = p;
	l_cameraGPUData.p_jittered = p;
	l_cameraGPUData.globalPos = m_probes[probeIndex].pos;
	auto l_t = InnoMath::getInvertTranslationMatrix(m_probes[probeIndex].pos);
	l_cameraGPUData.t = l_t;
	l_cameraGPUData.t_prev = l_t;

	// draw opaque meshes
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);

	glEnable(GL_DEPTH_CLAMP);

	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilMask(0xFF);
	glStencilFunc(GL_ALWAYS, 0x01, 0xFF);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	activateShaderProgram(GLOpaquePass::getGLSPC());

	l_cameraGPUData.r = v[faceIndex];
	l_cameraGPUData.r_prev = v[faceIndex];

	updateUBO(GLRenderingBackendComponent::get().m_cameraUBO, l_cameraGPUData);

	cleanRenderBuffers(m_opaquePassGLRPC);

	bindCubemapTextureForWrite(m_opaquePassGLRPC->m_GLTDCs[0], m_opaquePassGLRPC, 0, faceIndex, 0);
	bindCubemapTextureForWrite(m_opaquePassGLRPC->m_GLTDCs[1], m_opaquePassGLRPC, 1, faceIndex, 0);
	bindCubemapTextureForWrite(m_opaquePassGLRPC->m_GLTDCs[2], m_opaquePassGLRPC, 2, faceIndex, 0);
	bindCubemapTextureForWrite(m_opaquePassGLRPC->m_GLTDCs[3], m_opaquePassGLRPC, 3, faceIndex, 0);

	uint32_t l_offset = 0;

	for (uint32_t i = 0; i < g_pModuleManager->getRenderingFrontend()->getGIPassDrawCallCount(); i++)
	{
		auto l_GIPassGPUData = g_pModuleManager->getRenderingFrontend()->getGIPassGPUData()[i];

		if (l_GIPassGPUData.material->m_normalTexture)
		{
			activateTexture(reinterpret_cast<GLTextureDataComponent*>(l_GIPassGPUData.material->m_normalTexture), 0);
		}
		if (l_GIPassGPUData.material->m_albedoTexture)
		{
			activateTexture(reinterpret_cast<GLTextureDataComponent*>(l_GIPassGPUData.material->m_albedoTexture), 1);
		}
		if (l_GIPassGPUData.material->m_metallicTexture)
		{
			activateTexture(reinterpret_cast<GLTextureDataComponent*>(l_GIPassGPUData.material->m_metallicTexture), 2);
		}
		if (l_GIPassGPUData.material->m_roughnessTexture)
		{
			activateTexture(reinterpret_cast<GLTextureDataComponent*>(l_GIPassGPUData.material->m_roughnessTexture), 3);
		}
		if (l_GIPassGPUData.material->m_aoTexture)
		{
			activateTexture(reinterpret_cast<GLTextureDataComponent*>(l_GIPassGPUData.material->m_aoTexture), 4);
		}

		bindUBO(GLRenderingBackendComponent::get().m_meshUBO, 1, l_offset * sizeof(MeshGPUData), sizeof(MeshGPUData));
		bindUBO(GLRenderingBackendComponent::get().m_materialUBO, 2, l_offset * sizeof(MaterialGPUData), sizeof(MaterialGPUData));

		drawMesh(reinterpret_cast<GLMeshDataComponent*>(l_GIPassGPUData.mesh));

		l_offset++;
	}

	unbindCubemapTextureForWrite(m_opaquePassGLRPC, 0, faceIndex, 0);
	unbindCubemapTextureForWrite(m_opaquePassGLRPC, 1, faceIndex, 0);
	unbindCubemapTextureForWrite(m_opaquePassGLRPC, 2, faceIndex, 0);
	unbindCubemapTextureForWrite(m_opaquePassGLRPC, 3, faceIndex, 0);

	glDisable(GL_CULL_FACE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_CLAMP);
	glDisable(GL_DEPTH_TEST);

	return true;
}

bool GLEnvironmentCapturePass::readBackSurfelCaches(uint32_t probeIndex)
{
	auto l_posWSMetallic = readCubemapSamples(m_opaquePassGLRPC, m_opaquePassGLRPC->m_GLTDCs[0]);
	auto l_normalRoughness = readCubemapSamples(m_opaquePassGLRPC, m_opaquePassGLRPC->m_GLTDCs[1]);
	auto l_albedoAO = readCubemapSamples(m_opaquePassGLRPC, m_opaquePassGLRPC->m_GLTDCs[2]);

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

	m_probeCaches[probeIndex].surfelCaches = l_surfels;

	m_surfels.insert(m_surfels.end(), l_surfels.begin(), l_surfels.end());

	return true;
}

bool GLEnvironmentCapturePass::drawSkyVisibilityPass(const mat4& p, const std::vector<mat4>& v, uint32_t faceIndex)
{
	auto l_MDC = getGLMeshDataComponent(MeshShapeType::CUBE);

	auto l_capturePos = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	CameraGPUData l_cameraGPUData;
	l_cameraGPUData.p_original = p;
	l_cameraGPUData.p_jittered = p;
	l_cameraGPUData.globalPos = l_capturePos;
	auto l_t = InnoMath::getInvertTranslationMatrix(l_capturePos);
	l_cameraGPUData.t = l_t;
	l_cameraGPUData.t_prev = l_t;

	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_EQUAL, 0x01, 0xFF);
	glStencilMask(0x00);

	copyStencilBuffer(m_opaquePassGLRPC, m_skyVisibilityPassGLRPC);

	activateShaderProgram(m_skyVisibilityGLSPC);

	l_cameraGPUData.r = v[faceIndex];

	updateUBO(GLRenderingBackendComponent::get().m_cameraUBO, l_cameraGPUData);

	bindCubemapTextureForWrite(m_skyVisibilityPassGLRPC->m_GLTDCs[0], m_skyVisibilityPassGLRPC, 0, faceIndex, 0);

	drawMesh(l_MDC);

	unbindCubemapTextureForWrite(m_skyVisibilityPassGLRPC, 0, faceIndex, 0);

	glDisable(GL_STENCIL_TEST);

	return true;
}

bool GLEnvironmentCapturePass::drawSkyPass(const mat4& p, const std::vector<mat4>& v, uint32_t faceIndex)
{
	auto l_MDC = getGLMeshDataComponent(MeshShapeType::CUBE);

	vec4 l_probePos = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	CameraGPUData l_cameraGPUData;
	l_cameraGPUData.p_original = p;
	l_cameraGPUData.p_jittered = p;
	l_cameraGPUData.globalPos = l_probePos;
	auto l_t = InnoMath::getInvertTranslationMatrix(l_probePos);
	l_cameraGPUData.t = l_t;
	l_cameraGPUData.t_prev = l_t;

	SkyGPUData l_skyGPUData;
	l_skyGPUData.p_inv = p.inverse();
	l_skyGPUData.viewportSize = vec2((float)m_captureResolution, (float)m_captureResolution);

	activateShaderProgram(GLSkyPass::getGLSPC());

	l_cameraGPUData.r = v[faceIndex];
	l_cameraGPUData.r_prev = v[faceIndex];
	l_skyGPUData.r_inv = v[faceIndex].inverse();

	updateUBO(GLRenderingBackendComponent::get().m_skyUBO, l_skyGPUData);
	updateUBO(GLRenderingBackendComponent::get().m_cameraUBO, l_cameraGPUData);

	bindCubemapTextureForWrite(m_capturePassGLRPC->m_GLTDCs[0], m_capturePassGLRPC, 0, faceIndex, 0);

	drawMesh(l_MDC);

	unbindCubemapTextureForWrite(m_capturePassGLRPC, 0, faceIndex, 0);

	return true;
}

bool GLEnvironmentCapturePass::drawLightPass(const mat4& p, const std::vector<mat4>& v, uint32_t faceIndex)
{
	auto l_MDC = getGLMeshDataComponent(MeshShapeType::CUBE);

	auto capturePos = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	CameraGPUData l_cameraGPUData;
	l_cameraGPUData.p_original = p;
	l_cameraGPUData.p_jittered = p;
	l_cameraGPUData.globalPos = capturePos;
	auto l_t = InnoMath::getInvertTranslationMatrix(capturePos);
	l_cameraGPUData.t = l_t;
	l_cameraGPUData.t_prev = l_t;

	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_EQUAL, 0x01, 0xFF);
	glStencilMask(0x00);

	copyStencilBuffer(m_opaquePassGLRPC, m_capturePassGLRPC);

	activateShaderProgram(m_capturePassGLSPC);

	activateTexture(m_opaquePassGLRPC->m_GLTDCs[0], 0);
	activateTexture(m_opaquePassGLRPC->m_GLTDCs[1], 1);
	activateTexture(m_opaquePassGLRPC->m_GLTDCs[2], 2);

	l_cameraGPUData.r = v[faceIndex];

	updateUBO(GLRenderingBackendComponent::get().m_cameraUBO, l_cameraGPUData);

	bindCubemapTextureForWrite(m_capturePassGLRPC->m_GLTDCs[0], m_capturePassGLRPC, 0, faceIndex, 0);

	drawMesh(l_MDC);

	unbindCubemapTextureForWrite(m_capturePassGLRPC, 0, faceIndex, 0);

	glDisable(GL_STENCIL_TEST);

	return true;
}

bool GLEnvironmentCapturePass::eliminateDuplicatedSurfels()
{
	m_surfels.erase(std::unique(m_surfels.begin(), m_surfels.end()), m_surfels.end());
	m_surfels.shrink_to_fit();

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

	return true;
}

bool GLEnvironmentCapturePass::eliminateEmptyBricks()
{
	for (size_t i = 0; i < m_brickCaches.size(); i++)
	{
		if (!isBrickEmpty(m_surfels, m_brickCaches[i]))
		{
			m_bricks.emplace_back(m_brickCaches[i]);
		}
	}

	m_bricks.shrink_to_fit();

	std::sort(m_bricks.begin(), m_bricks.end(), [&](Brick A, Brick B)
	{
		if (A.boundBox.m_center.x != B.boundBox.m_center.x) {
			return A.boundBox.m_center.x < B.boundBox.m_center.x;
		}
		if (A.boundBox.m_center.y != B.boundBox.m_center.y) {
			return A.boundBox.m_center.y < B.boundBox.m_center.y;
		}
		return A.boundBox.m_center.z < B.boundBox.m_center.z;
	});

	return true;
}

bool GLEnvironmentCapturePass::isBrickEmpty(const std::vector<Surfel>& surfels, const Brick& brick)
{
	auto l_nearestSurfel = std::find_if(surfels.begin(), surfels.end(), [&](Surfel val) {
		return InnoMath::isAGreaterThanBVec3(val.pos, brick.boundBox.m_boundMin);
	});

	if (l_nearestSurfel != surfels.end())
	{
		auto l_farestSurfel = std::find_if(surfels.begin(), surfels.end(), [&](Surfel val) {
			return InnoMath::isALessThanBVec3(val.pos, brick.boundBox.m_boundMax);
		});

		if (l_farestSurfel != surfels.end())
		{
			return false;
		}
	}

	return true;
}

bool GLEnvironmentCapturePass::assignSurfelRangeToBricks()
{
	for (size_t i = 0; i < m_bricks.size(); i++)
	{
		findSurfelRangeForBrick(m_bricks[i]);
	}

	return true;
}

bool GLEnvironmentCapturePass::findSurfelRangeForBrick(Brick& brick)
{
	auto l_firstSurfel = std::find_if(m_surfels.begin(), m_surfels.end(), [&](Surfel val) {
		return InnoMath::isAGreaterThanBVec3(val.pos, brick.boundBox.m_boundMin);
	});

	auto l_firstSurfelIndex = std::distance(m_surfels.begin(), l_firstSurfel);
	brick.surfelRangeBegin = (uint32_t)l_firstSurfelIndex;

	auto l_lastSurfel = std::find_if(m_surfels.begin(), m_surfels.end(), [&](Surfel val) {
		return InnoMath::isAGreaterThanBVec3(val.pos, brick.boundBox.m_boundMax);
	});

	auto l_lastSurfelIndex = std::distance(m_surfels.begin(), l_lastSurfel);
	brick.surfelRangeEnd = (uint32_t)l_lastSurfelIndex;

	return true;
}

bool GLEnvironmentCapturePass::assignBrickFactorToProbes()
{
	for (size_t i = 0; i < m_probes.size(); i++)
	{
		m_probes[i].brickFactorRangeBegin = (uint32_t)m_brickFactors.size();

		for (size_t j = 0; j < m_bricks.size(); j++)
		{
			if (!isBrickEmpty(m_probeCaches[i].surfelCaches, m_bricks[j]))
			{
				BrickFactor l_brickFactor;
				l_brickFactor.basisWeight = 1.0f;
				l_brickFactor.brickIndex = (uint32_t)j;

				m_brickFactors.emplace_back(l_brickFactor);
			}
		}

		m_probes[i].brickFactorRangeEnd = (uint32_t)m_brickFactors.size() - 1;
	}

	return true;
}

bool GLEnvironmentCapturePass::serializeProbes()
{
	auto l_filePath = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_currentSceneName = g_pModuleManager->getFileSystem()->getCurrentSceneName();

	std::ofstream l_file;
	l_file.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoProbe", std::ios::binary);
	IOService::serializeVector(l_file, m_probes);

	return true;
}

bool GLEnvironmentCapturePass::serializeSurfels()
{
	auto l_filePath = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_currentSceneName = g_pModuleManager->getFileSystem()->getCurrentSceneName();

	std::ofstream l_file;
	l_file.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoSurfel", std::ios::binary);
	IOService::serializeVector(l_file, m_surfels);

	return true;
}

bool GLEnvironmentCapturePass::serializeBricks()
{
	auto l_filePath = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_currentSceneName = g_pModuleManager->getFileSystem()->getCurrentSceneName();

	std::ofstream l_file;
	l_file.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoBrick", std::ios::binary);
	IOService::serializeVector(l_file, m_bricks);

	return true;
}

bool GLEnvironmentCapturePass::serializeBrickFactors()
{
	auto l_filePath = g_pModuleManager->getFileSystem()->getWorkingDirectory();
	auto l_currentSceneName = g_pModuleManager->getFileSystem()->getCurrentSceneName();

	std::ofstream l_file;
	l_file.open(l_filePath + "//Res//Scenes//" + l_currentSceneName + ".InnoBrickFactor", std::ios::binary);
	IOService::serializeVector(l_file, m_brickFactors);

	return true;
}

bool GLEnvironmentCapturePass::update()
{
	if (!loadGIData())
	{
		updateUBO(GLRenderingBackendComponent::get().m_meshUBO, g_pModuleManager->getRenderingFrontend()->getGIPassMeshGPUData());
		updateUBO(GLRenderingBackendComponent::get().m_materialUBO, g_pModuleManager->getRenderingFrontend()->getGIPassMaterialGPUData());

		m_probeCaches.clear();
		m_probes.clear();
		m_surfels.clear();
		m_bricks.clear();
		m_brickCaches.clear();
		m_brickFactors.clear();

		generateProbes();
		generateBricks();

		capture();

		eliminateDuplicatedSurfels();
		eliminateEmptyBricks();

		assignSurfelRangeToBricks();
		assignBrickFactorToProbes();

		serializeProbes();
		serializeSurfels();
		serializeBricks();
		serializeBrickFactors();
	}

	return true;
}

bool GLEnvironmentCapturePass::reloadShader()
{
	return true;
}

GLRenderPassComponent * GLEnvironmentCapturePass::getGLRPC()
{
	return m_capturePassGLRPC;
}

const std::vector<Probe>& GLEnvironmentCapturePass::getProbes()
{
	return m_probes;
}

const std::vector<Brick>& GLEnvironmentCapturePass::getBricks()
{
	return m_bricks;
}