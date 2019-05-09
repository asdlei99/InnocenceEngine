#include "GLRenderingSystem.h"

#include "GLRenderingSystemUtilities.h"

#include "GLBRDFLUTPass.h"
#include "GLEnvironmentCapturePass.h"
#include "GLEnvironmentConvolutionPass.h"
#include "GLEnvironmentPreFilterPass.h"

#include "GLVXGIPass.h"

#include "GLShadowPass.h"

#include "GLEarlyZPass.h"
#include "GLOpaquePass.h"
#include "GLSSAONoisePass.h"
#include "GLSSAOBlurPass.h"
#include "GLTerrainPass.h"

#include "GLLightCullingPass.h"
#include "GLLightPass.h"

#include "GLSkyPass.h"
#include "GLPreTAAPass.h"
#include "GLTransparentPass.h"

#include "GLTAAPass.h"
#include "GLPostTAAPass.h"
#include "GLMotionBlurPass.h"
#include "GLBloomExtractPass.h"
#include "GLBloomBlurPass.h"
#include "GLBloomMergePass.h"
#include "GLBillboardPass.h"
#include "GLDebuggerPass.h"
#include "GLFinalBlendPass.h"

#include "../../component/GLRenderingSystemComponent.h"
#include "../../component/RenderingFrontendSystemComponent.h"

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE GLRenderingSystemNS
{
	void MessageCallback(GLenum source,
		GLenum type,
		GLuint id,
		GLenum severity,
		GLsizei length,
		const GLchar* message,
		const void* userParam)
	{
		if (severity == GL_DEBUG_SEVERITY_HIGH)
		{
			LogType l_logType;
			std::string l_typeStr;
			if (type == GL_DEBUG_TYPE_ERROR)
			{
				l_logType = LogType::INNO_ERROR;
				l_typeStr = "GL_DEBUG_TYPE_ERROR: ID: ";
			}
			else if (type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
			{
				l_logType = LogType::INNO_ERROR;
				l_typeStr = "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: ID: ";
			}
			else if (type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR)
			{
				l_logType = LogType::INNO_ERROR;
				l_typeStr = "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: ID: ";
			}
			else if (type == GL_DEBUG_TYPE_PERFORMANCE)
			{
				l_logType = LogType::INNO_ERROR;
				l_typeStr = "GL_DEBUG_TYPE_PERFORMANCE: ID: ";
			}
			else if (type == GL_DEBUG_TYPE_PORTABILITY)
			{
				l_logType = LogType::INNO_ERROR;
				l_typeStr = "GL_DEBUG_TYPE_PORTABILITY: ID: ";
			}
			else if (type == GL_DEBUG_TYPE_OTHER)
			{
				l_logType = LogType::INNO_ERROR;
				l_typeStr = "GL_DEBUG_TYPE_OTHER: ID: ";
			}
			else
			{
				l_logType = LogType::INNO_DEV_VERBOSE;
			}

			std::string l_message = message;
			g_pCoreSystem->getLogSystem()->printLog(l_logType, "GLRenderingSystem: " + l_typeStr + std::to_string(id) + ": " + l_message);
		}
	}

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;

	std::function<void()> f_sceneLoadingFinishCallback;

	bool m_isBaked = false;
	bool m_visualizeVXGI = false;
	std::function<void()> f_toggleVisualizeVXGI;

	ThreadSafeUnorderedMap<EntityID, GLMeshDataComponent*> m_meshMap;
	ThreadSafeUnorderedMap<EntityID, MaterialDataComponent*> m_materialMap;
	ThreadSafeUnorderedMap<EntityID, GLTextureDataComponent*> m_textureMap;

	void* m_MeshDataComponentPool;
	void* m_MaterialDataComponentPool;
	void* m_TextureDataComponentPool;

	ThreadSafeQueue<GLMeshDataComponent*> m_uninitializedMDC;
	ThreadSafeQueue<GLTextureDataComponent*> m_uninitializedTDC;

	GLTextureDataComponent* m_iconTemplate_OBJ;
	GLTextureDataComponent* m_iconTemplate_PNG;
	GLTextureDataComponent* m_iconTemplate_SHADER;
	GLTextureDataComponent* m_iconTemplate_UNKNOWN;

	GLTextureDataComponent* m_iconTemplate_DirectionalLight;
	GLTextureDataComponent* m_iconTemplate_PointLight;
	GLTextureDataComponent* m_iconTemplate_SphereLight;

	GLMeshDataComponent* m_unitLineMDC;
	GLMeshDataComponent* m_unitQuadMDC;
	GLMeshDataComponent* m_unitCubeMDC;
	GLMeshDataComponent* m_unitSphereMDC;
	GLMeshDataComponent* m_terrainMDC;

	GLTextureDataComponent* m_basicNormalTDC;
	GLTextureDataComponent* m_basicAlbedoTDC;
	GLTextureDataComponent* m_basicMetallicTDC;
	GLTextureDataComponent* m_basicRoughnessTDC;
	GLTextureDataComponent* m_basicAOTDC;
}

bool GLRenderingSystemNS::setup()
{
	initializeComponentPool();

	f_sceneLoadingFinishCallback = [&]() {
		m_isBaked = false;
	};

	f_toggleVisualizeVXGI = [&]() {
		m_visualizeVXGI = !m_visualizeVXGI;
	};
	g_pCoreSystem->getInputSystem()->addButtonStatusCallback(ButtonData{ INNO_KEY_G, ButtonStatus::PRESSED }, &f_toggleVisualizeVXGI);

	g_pCoreSystem->getFileSystem()->addSceneLoadingFinishCallback(&f_sceneLoadingFinishCallback);

	auto l_screenResolution = g_pCoreSystem->getVisionSystem()->getRenderingFrontend()->getScreenResolution();

	// general render pass desc
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTNumber = 1;
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.samplerType = TextureSamplerType::SAMPLER_2D;
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.usageType = TextureUsageType::COLOR_ATTACHMENT;
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.pixelDataFormat = TexturePixelDataFormat::RGBA;
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.minFilterMethod = TextureFilterMethod::NEAREST;
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.magFilterMethod = TextureFilterMethod::NEAREST;
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.wrapMethod = TextureWrapMethod::CLAMP_TO_EDGE;
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.width = l_screenResolution.x;
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.height = l_screenResolution.y;
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.pixelDataType = TexturePixelDataType::FLOAT16;

	if (g_pCoreSystem->getVisionSystem()->getRenderingFrontend()->getRenderingConfig().MSAAdepth)
	{
		// antialiasing
		// MSAA
		glEnable(GL_MULTISAMPLE);
	}

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(MessageCallback, 0);

	// enable seamless cubemap sampling for lower mip levels in the pre-filter map.
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glEnable(GL_PROGRAM_POINT_SIZE);

	m_objectStatus = ObjectStatus::ALIVE;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "GLRenderingSystem setup finished.");
	return true;
}

bool GLRenderingSystemNS::initialize()
{
	m_MeshDataComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(GLMeshDataComponent), RenderingFrontendSystemComponent::get().m_maxMeshes);
	m_MaterialDataComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(MaterialDataComponent), RenderingFrontendSystemComponent::get().m_maxMaterials);
	m_TextureDataComponentPool = g_pCoreSystem->getMemorySystem()->allocateMemoryPool(sizeof(GLTextureDataComponent), RenderingFrontendSystemComponent::get().m_maxTextures);

	loadDefaultAssets();

	generateGPUBuffers();

	GLBRDFLUTPass::initialize();
	GLEnvironmentCapturePass::initialize();
	GLEnvironmentConvolutionPass::initialize();
	GLEnvironmentPreFilterPass::initialize();
	GLVXGIPass::initialize();

	GLShadowPass::initialize();

	GLEarlyZPass::initialize();
	GLOpaquePass::initialize();
	GLSSAONoisePass::initialize();
	GLSSAOBlurPass::initialize();
	GLTerrainPass::initialize();

	GLLightCullingPass::initialize();
	GLLightPass::initialize();

	GLSkyPass::initialize();
	GLPreTAAPass::initialize();
	GLTransparentPass::initialize();

	GLTAAPass::initialize();
	GLPostTAAPass::initialize();
	GLMotionBlurPass::initialize();
	GLBloomExtractPass::initialize();
	GLBloomBlurPass::initialize();
	GLBloomMergePass::initialize();
	GLBillboardPass::initialize();
	GLDebuggerPass::initialize();
	GLFinalBlendPass::initialize();

	return true;
}

void  GLRenderingSystemNS::loadDefaultAssets()
{
	auto l_basicNormalTDC = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//basic_normal.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_basicAlbedoTDC = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//basic_albedo.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::ALBEDO);
	auto l_basicMetallicTDC = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//basic_metallic.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::METALLIC);
	auto l_basicRoughnessTDC = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//basic_roughness.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::ROUGHNESS);
	auto l_basicAOTDC = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//basic_ao.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::AMBIENT_OCCLUSION);

	auto l_iconTemplate_OBJ = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoFileTypeIcons_OBJ.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_PNG = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoFileTypeIcons_PNG.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_SHADER = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoFileTypeIcons_SHADER.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_UNKNOWN = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoFileTypeIcons_UNKNOWN.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);

	auto l_iconTemplate_DirectionalLight = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoWorldEditorIcons_DirectionalLight.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_PointLight = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoWorldEditorIcons_PointLight.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);
	auto l_iconTemplate_SphereLight = g_pCoreSystem->getAssetSystem()->loadTexture("res//textures//InnoWorldEditorIcons_SphereLight.png", TextureSamplerType::SAMPLER_2D, TextureUsageType::NORMAL);

	m_basicNormalTDC = reinterpret_cast<GLTextureDataComponent*>(l_basicNormalTDC);
	m_basicAlbedoTDC = reinterpret_cast<GLTextureDataComponent*>(l_basicAlbedoTDC);
	m_basicMetallicTDC = reinterpret_cast<GLTextureDataComponent*>(l_basicMetallicTDC);
	m_basicRoughnessTDC = reinterpret_cast<GLTextureDataComponent*>(l_basicRoughnessTDC);
	m_basicAOTDC = reinterpret_cast<GLTextureDataComponent*>(l_basicAOTDC);

	m_iconTemplate_OBJ = reinterpret_cast<GLTextureDataComponent*>(l_iconTemplate_OBJ);
	m_iconTemplate_PNG = reinterpret_cast<GLTextureDataComponent*>(l_iconTemplate_PNG);
	m_iconTemplate_SHADER = reinterpret_cast<GLTextureDataComponent*>(l_iconTemplate_SHADER);
	m_iconTemplate_UNKNOWN = reinterpret_cast<GLTextureDataComponent*>(l_iconTemplate_UNKNOWN);

	m_iconTemplate_DirectionalLight = reinterpret_cast<GLTextureDataComponent*>(l_iconTemplate_DirectionalLight);
	m_iconTemplate_PointLight = reinterpret_cast<GLTextureDataComponent*>(l_iconTemplate_PointLight);
	m_iconTemplate_SphereLight = reinterpret_cast<GLTextureDataComponent*>(l_iconTemplate_SphereLight);

	m_unitLineMDC = addGLMeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitLine(*m_unitLineMDC);
	m_unitLineMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE_STRIP;
	m_unitLineMDC->m_meshShapeType = MeshShapeType::LINE;
	m_unitLineMDC->m_objectStatus = ObjectStatus::STANDBY;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitLineMDC);

	m_unitQuadMDC = addGLMeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitQuad(*m_unitQuadMDC);
	m_unitQuadMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_unitQuadMDC->m_meshShapeType = MeshShapeType::QUAD;
	m_unitQuadMDC->m_objectStatus = ObjectStatus::STANDBY;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitQuadMDC);

	m_unitCubeMDC = addGLMeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitCube(*m_unitCubeMDC);
	m_unitCubeMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_unitCubeMDC->m_meshShapeType = MeshShapeType::CUBE;
	m_unitCubeMDC->m_objectStatus = ObjectStatus::STANDBY;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitCubeMDC);

	m_unitSphereMDC = addGLMeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addUnitSphere(*m_unitSphereMDC);
	m_unitSphereMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_unitSphereMDC->m_meshShapeType = MeshShapeType::SPHERE;
	m_unitSphereMDC->m_objectStatus = ObjectStatus::STANDBY;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_unitSphereMDC);

	m_terrainMDC = addGLMeshDataComponent();
	g_pCoreSystem->getAssetSystem()->addTerrain(*m_terrainMDC);
	m_terrainMDC->m_meshPrimitiveTopology = MeshPrimitiveTopology::TRIANGLE;
	m_terrainMDC->m_objectStatus = ObjectStatus::STANDBY;
	g_pCoreSystem->getPhysicsSystem()->generatePhysicsDataComponent(m_terrainMDC);

	initializeGLMeshDataComponent(m_unitLineMDC);
	initializeGLMeshDataComponent(m_unitQuadMDC);
	initializeGLMeshDataComponent(m_unitCubeMDC);
	initializeGLMeshDataComponent(m_unitSphereMDC);
	initializeGLMeshDataComponent(m_terrainMDC);

	initializeGLTextureDataComponent(m_basicNormalTDC);
	initializeGLTextureDataComponent(m_basicAlbedoTDC);
	initializeGLTextureDataComponent(m_basicMetallicTDC);
	initializeGLTextureDataComponent(m_basicRoughnessTDC);
	initializeGLTextureDataComponent(m_basicAOTDC);

	initializeGLTextureDataComponent(m_iconTemplate_OBJ);
	initializeGLTextureDataComponent(m_iconTemplate_PNG);
	initializeGLTextureDataComponent(m_iconTemplate_SHADER);
	initializeGLTextureDataComponent(m_iconTemplate_UNKNOWN);

	initializeGLTextureDataComponent(m_iconTemplate_DirectionalLight);
	initializeGLTextureDataComponent(m_iconTemplate_PointLight);
	initializeGLTextureDataComponent(m_iconTemplate_SphereLight);
}

bool GLRenderingSystemNS::generateGPUBuffers()
{
	GLRenderingSystemComponent::get().m_cameraUBO = generateUBO(sizeof(CameraGPUData), 0);

	GLRenderingSystemComponent::get().m_meshUBO = generateUBO(sizeof(MeshGPUData), 1);

	GLRenderingSystemComponent::get().m_materialUBO = generateUBO(sizeof(MaterialGPUData), 2);

	GLRenderingSystemComponent::get().m_sunUBO = generateUBO(sizeof(SunGPUData), 3);

	GLRenderingSystemComponent::get().m_pointLightUBO = generateUBO(sizeof(PointLightGPUData) * RenderingFrontendSystemComponent::get().m_maxPointLights, 4);

	GLRenderingSystemComponent::get().m_sphereLightUBO = generateUBO(sizeof(SphereLightGPUData) * RenderingFrontendSystemComponent::get().m_maxSphereLights, 5);

	GLRenderingSystemComponent::get().m_CSMUBO = generateUBO(sizeof(CSMGPUData) * RenderingFrontendSystemComponent::get().m_maxCSMSplit, 6);

	GLRenderingSystemComponent::get().m_skyUBO = generateUBO(sizeof(SkyGPUData), 7);

	GLRenderingSystemComponent::get().m_dispatchParamsUBO = generateUBO(sizeof(DispatchParamsGPUData), 8);

	return true;
}

bool GLRenderingSystemNS::update()
{
	if (GLRenderingSystemNS::m_uninitializedMDC.size() > 0)
	{
		GLMeshDataComponent* l_MDC;
		GLRenderingSystemNS::m_uninitializedMDC.tryPop(l_MDC);

		if (l_MDC)
		{
			auto l_result = initializeGLMeshDataComponent(l_MDC);
			if (!l_result)
			{
				g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "GLRenderingSystem: can't create GLMeshDataComponent for " + std::string(l_MDC->m_parentEntity.c_str()) + "!");
			}
		}
	}
	if (GLRenderingSystemNS::m_uninitializedTDC.size() > 0)
	{
		GLTextureDataComponent* l_TDC;
		GLRenderingSystemNS::m_uninitializedTDC.tryPop(l_TDC);

		if (l_TDC)
		{
			auto l_result = initializeGLTextureDataComponent(l_TDC);
			if (!l_result)
			{
				g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "GLRenderingSystem: can't create GLTextureDataComponent for " + std::string(l_TDC->m_parentEntity.c_str()) + "!");
			}
		}
	}

	return true;
}

bool GLRenderingSystemNS::render()
{
	glFrontFace(GL_CCW);

	updateUBO(GLRenderingSystemComponent::get().m_cameraUBO, RenderingFrontendSystemComponent::get().m_cameraGPUData);
	updateUBO(GLRenderingSystemComponent::get().m_sunUBO, RenderingFrontendSystemComponent::get().m_sunGPUData);
	updateUBO(GLRenderingSystemComponent::get().m_pointLightUBO, RenderingFrontendSystemComponent::get().m_pointLightGPUDataVector);
	updateUBO(GLRenderingSystemComponent::get().m_sphereLightUBO, RenderingFrontendSystemComponent::get().m_sphereLightGPUDataVector);
	updateUBO(GLRenderingSystemComponent::get().m_CSMUBO, RenderingFrontendSystemComponent::get().m_CSMGPUDataVector);
	updateUBO(GLRenderingSystemComponent::get().m_skyUBO, RenderingFrontendSystemComponent::get().m_skyGPUData);

	if (!m_isBaked)
	{
		GLEnvironmentCapturePass::update();
		GLEnvironmentConvolutionPass::update();
		GLEnvironmentPreFilterPass::update();
		GLVXGIPass::update();
		m_isBaked = true;
	}

	GLShadowPass::update();

	GLEarlyZPass::update();
	GLOpaquePass::update();
	GLSSAONoisePass::update();
	GLSSAOBlurPass::update();

	GLTerrainPass::update();

	GLLightCullingPass::update();
	GLLightPass::update();

	auto l_renderingConfig = g_pCoreSystem->getVisionSystem()->getRenderingFrontend()->getRenderingConfig();

	GLSkyPass::update();
	GLPreTAAPass::update();
	GLTransparentPass::update();

	auto l_canvasGLRPC = GLPreTAAPass::getGLRPC();

	if (l_renderingConfig.useBloom)
	{
		GLBloomExtractPass::update(l_canvasGLRPC);

		GLBloomBlurPass::update(GLBloomExtractPass::getGLRPC(0));

		GLBloomBlurPass::update(GLBloomExtractPass::getGLRPC(1));

		GLBloomBlurPass::update(GLBloomExtractPass::getGLRPC(2));

		GLBloomBlurPass::update(GLBloomExtractPass::getGLRPC(3));

		GLBloomMergePass::update();

		l_canvasGLRPC = GLBloomMergePass::getGLRPC();
	}
	else
	{
		cleanRenderBuffers(GLBloomExtractPass::getGLRPC(0));
		cleanRenderBuffers(GLBloomExtractPass::getGLRPC(1));
		cleanRenderBuffers(GLBloomExtractPass::getGLRPC(2));
		cleanRenderBuffers(GLBloomExtractPass::getGLRPC(3));

		cleanRenderBuffers(GLBloomBlurPass::getGLRPC(0));
		cleanRenderBuffers(GLBloomBlurPass::getGLRPC(1));

		cleanRenderBuffers(GLBloomMergePass::getGLRPC());
	}

	if (l_renderingConfig.useTAA)
	{
		GLTAAPass::update(l_canvasGLRPC);
		GLPostTAAPass::update();

		l_canvasGLRPC = GLPostTAAPass::getGLRPC();
	}

	if (l_renderingConfig.useMotionBlur)
	{
		GLMotionBlurPass::update();

		l_canvasGLRPC = GLMotionBlurPass::getGLRPC();
	}

	GLBillboardPass::update();

	if (l_renderingConfig.drawDebugObject)
	{
		GLDebuggerPass::update();
	}
	else
	{
		cleanRenderBuffers(GLDebuggerPass::getGLRPC(0));
	}

	//if (m_visualizeVXGI)
	//{
	//	GLVXGIPass::draw();
	//	l_canvasGLRPC = GLVXGIPass::getGLRPC();
	//}

	GLFinalBlendPass::update(l_canvasGLRPC);

	return true;
}

bool GLRenderingSystemNS::terminate()
{
	m_objectStatus = ObjectStatus::SHUTDOWN;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "GLRenderingSystem has been terminated.");
	return true;
}

GLMeshDataComponent* GLRenderingSystemNS::addGLMeshDataComponent()
{
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_MeshDataComponentPool, sizeof(GLMeshDataComponent));
	auto l_MDC = new(l_rawPtr)GLMeshDataComponent();
	auto l_parentEntity = InnoMath::createEntityID();
	l_MDC->m_parentEntity = l_parentEntity;
	auto l_meshMap = &m_meshMap;
	l_meshMap->emplace(std::pair<EntityID, GLMeshDataComponent*>(l_parentEntity, l_MDC));
	return l_MDC;
}

MaterialDataComponent* GLRenderingSystemNS::addMaterialDataComponent()
{
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_MaterialDataComponentPool, sizeof(MaterialDataComponent));
	auto l_MDC = new(l_rawPtr)MaterialDataComponent();
	auto l_parentEntity = InnoMath::createEntityID();
	l_MDC->m_parentEntity = l_parentEntity;
	auto l_materialMap = &m_materialMap;
	l_materialMap->emplace(std::pair<EntityID, MaterialDataComponent*>(l_parentEntity, l_MDC));
	return l_MDC;
}

GLTextureDataComponent* GLRenderingSystemNS::addGLTextureDataComponent()
{
	auto l_rawPtr = g_pCoreSystem->getMemorySystem()->spawnObject(m_TextureDataComponentPool, sizeof(GLTextureDataComponent));
	auto l_TDC = new(l_rawPtr)GLTextureDataComponent();
	auto l_parentEntity = InnoMath::createEntityID();
	l_TDC->m_parentEntity = l_parentEntity;
	auto l_textureMap = &m_textureMap;
	l_textureMap->emplace(std::pair<EntityID, GLTextureDataComponent*>(l_parentEntity, l_TDC));
	return l_TDC;
}

GLMeshDataComponent* GLRenderingSystemNS::getGLMeshDataComponent(EntityID entityID)
{
	auto result = GLRenderingSystemNS::m_meshMap.find(entityID);
	if (result != GLRenderingSystemNS::m_meshMap.end())
	{
		return result->second;
	}
	else
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackendSystem: can't find MeshDataComponent by EntityID: " + std::string(entityID.c_str()) + " !");
		return nullptr;
	}
}

GLTextureDataComponent * GLRenderingSystemNS::getGLTextureDataComponent(EntityID entityID)
{
	auto result = GLRenderingSystemNS::m_textureMap.find(entityID);
	if (result != GLRenderingSystemNS::m_textureMap.end())
	{
		return result->second;
	}
	else
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackendSystem: can't find TextureDataComponent by EntityID: " + std::string(entityID.c_str()) + " !");
		return nullptr;
	}
}

GLMeshDataComponent* GLRenderingSystemNS::getGLMeshDataComponent(MeshShapeType meshShapeType)
{
	switch (meshShapeType)
	{
	case MeshShapeType::LINE:
		return GLRenderingSystemNS::m_unitLineMDC; break;
	case MeshShapeType::QUAD:
		return GLRenderingSystemNS::m_unitQuadMDC; break;
	case MeshShapeType::CUBE:
		return GLRenderingSystemNS::m_unitCubeMDC; break;
	case MeshShapeType::SPHERE:
		return GLRenderingSystemNS::m_unitSphereMDC; break;
	case MeshShapeType::TERRAIN:
		return GLRenderingSystemNS::m_terrainMDC; break;
	case MeshShapeType::CUSTOM:
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackendSystem: wrong MeshShapeType passed to GLRenderingSystem::getMeshDataComponent() !");
		return nullptr; break;
	default:
		return nullptr; break;
	}
}

GLTextureDataComponent * GLRenderingSystemNS::getGLTextureDataComponent(TextureUsageType textureUsageType)
{
	switch (textureUsageType)
	{
	case TextureUsageType::INVISIBLE:
		return nullptr; break;
	case TextureUsageType::NORMAL:
		return GLRenderingSystemNS::m_basicNormalTDC; break;
	case TextureUsageType::ALBEDO:
		return GLRenderingSystemNS::m_basicAlbedoTDC; break;
	case TextureUsageType::METALLIC:
		return GLRenderingSystemNS::m_basicMetallicTDC; break;
	case TextureUsageType::ROUGHNESS:
		return GLRenderingSystemNS::m_basicRoughnessTDC; break;
	case TextureUsageType::AMBIENT_OCCLUSION:
		return GLRenderingSystemNS::m_basicAOTDC; break;
	case TextureUsageType::COLOR_ATTACHMENT:
		return nullptr; break;
	default:
		return nullptr; break;
	}
}

GLTextureDataComponent * GLRenderingSystemNS::getGLTextureDataComponent(FileExplorerIconType iconType)
{
	switch (iconType)
	{
	case FileExplorerIconType::OBJ:
		return GLRenderingSystemNS::m_iconTemplate_OBJ; break;
	case FileExplorerIconType::PNG:
		return GLRenderingSystemNS::m_iconTemplate_PNG; break;
	case FileExplorerIconType::SHADER:
		return GLRenderingSystemNS::m_iconTemplate_SHADER; break;
	case FileExplorerIconType::UNKNOWN:
		return GLRenderingSystemNS::m_iconTemplate_UNKNOWN; break;
	default:
		return nullptr; break;
	}
}

GLTextureDataComponent * GLRenderingSystemNS::getGLTextureDataComponent(WorldEditorIconType iconType)
{
	switch (iconType)
	{
	case WorldEditorIconType::DIRECTIONAL_LIGHT:
		return GLRenderingSystemNS::m_iconTemplate_DirectionalLight; break;
	case WorldEditorIconType::POINT_LIGHT:
		return GLRenderingSystemNS::m_iconTemplate_PointLight; break;
	case WorldEditorIconType::SPHERE_LIGHT:
		return GLRenderingSystemNS::m_iconTemplate_SphereLight; break;
	default:
		return nullptr; break;
	}
}

bool GLRenderingSystemNS::resize()
{
	auto l_screenResolution = g_pCoreSystem->getVisionSystem()->getRenderingFrontend()->getScreenResolution();

	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.width = l_screenResolution.x;
	GLRenderingSystemComponent::get().m_deferredRenderPassDesc.RTDesc.height = l_screenResolution.y;

	GLEarlyZPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLOpaquePass::resize(l_screenResolution.x, l_screenResolution.y);
	GLSSAONoisePass::resize(l_screenResolution.x, l_screenResolution.y);
	GLSSAOBlurPass::resize(l_screenResolution.x, l_screenResolution.y);

	GLTerrainPass::resize(l_screenResolution.x, l_screenResolution.y);

	GLLightCullingPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLLightPass::resize(l_screenResolution.x, l_screenResolution.y);

	GLSkyPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLPreTAAPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLTransparentPass::resize(l_screenResolution.x, l_screenResolution.y);

	GLTAAPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLPostTAAPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLMotionBlurPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLBloomExtractPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLBloomBlurPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLBloomMergePass::resize(l_screenResolution.x, l_screenResolution.y);
	GLBillboardPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLDebuggerPass::resize(l_screenResolution.x, l_screenResolution.y);
	GLFinalBlendPass::resize(l_screenResolution.x, l_screenResolution.y);

	return true;
}

bool GLRenderingSystem::setup()
{
	return GLRenderingSystemNS::setup();
}

bool GLRenderingSystem::initialize()
{
	return GLRenderingSystemNS::initialize();
}

bool GLRenderingSystem::update()
{
	return GLRenderingSystemNS::update();
}

bool GLRenderingSystem::render()
{
	return GLRenderingSystemNS::render();
}

bool GLRenderingSystem::terminate()
{
	return GLRenderingSystemNS::terminate();
}

ObjectStatus GLRenderingSystem::getStatus()
{
	return GLRenderingSystemNS::m_objectStatus;
}

MeshDataComponent * GLRenderingSystem::addMeshDataComponent()
{
	return GLRenderingSystemNS::addGLMeshDataComponent();
}

MaterialDataComponent * GLRenderingSystem::addMaterialDataComponent()
{
	return GLRenderingSystemNS::addMaterialDataComponent();
}

TextureDataComponent * GLRenderingSystem::addTextureDataComponent()
{
	return GLRenderingSystemNS::addGLTextureDataComponent();
}

MeshDataComponent * GLRenderingSystem::getMeshDataComponent(EntityID meshID)
{
	return GLRenderingSystemNS::getGLMeshDataComponent(meshID);
}

TextureDataComponent * GLRenderingSystem::getTextureDataComponent(EntityID textureID)
{
	return GLRenderingSystemNS::getGLTextureDataComponent(textureID);
}

MeshDataComponent * GLRenderingSystem::getMeshDataComponent(MeshShapeType MeshShapeType)
{
	return GLRenderingSystemNS::getGLMeshDataComponent(MeshShapeType);
}

TextureDataComponent * GLRenderingSystem::getTextureDataComponent(TextureUsageType TextureUsageType)
{
	return GLRenderingSystemNS::getGLTextureDataComponent(TextureUsageType);
}

TextureDataComponent * GLRenderingSystem::getTextureDataComponent(FileExplorerIconType iconType)
{
	return GLRenderingSystemNS::getGLTextureDataComponent(iconType);
}

TextureDataComponent * GLRenderingSystem::getTextureDataComponent(WorldEditorIconType iconType)
{
	return GLRenderingSystemNS::getGLTextureDataComponent(iconType);
}

bool GLRenderingSystem::removeMeshDataComponent(EntityID entityID)
{
	auto l_meshMap = &GLRenderingSystemNS::m_meshMap;
	auto l_mesh = l_meshMap->find(entityID);
	if (l_mesh != l_meshMap->end())
	{
		g_pCoreSystem->getMemorySystem()->destroyObject(GLRenderingSystemNS::m_MeshDataComponentPool, sizeof(GLMeshDataComponent), l_mesh->second);
		l_meshMap->erase(entityID);
		return true;
	}
	else
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackendSystem: can't remove MeshDataComponent by EntityID: " + std::string(entityID.c_str()) + " !");
		return false;
	}
}

bool GLRenderingSystem::removeTextureDataComponent(EntityID entityID)
{
	auto l_textureMap = &GLRenderingSystemNS::m_textureMap;
	auto l_texture = l_textureMap->find(entityID);
	if (l_texture != l_textureMap->end())
	{
		for (auto& i : l_texture->second->m_textureData)
		{
			// @TODO
		}

		g_pCoreSystem->getMemorySystem()->destroyObject(GLRenderingSystemNS::m_TextureDataComponentPool, sizeof(GLTextureDataComponent), l_texture->second);
		l_textureMap->erase(entityID);
		return true;
	}
	else
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "RenderingBackendSystem: can't remove TextureDataComponent by EntityID: " + std::string(entityID.c_str()) + " !");
		return false;
	}
}

void GLRenderingSystem::registerUninitializedMeshDataComponent(MeshDataComponent * rhs)
{
	GLRenderingSystemNS::m_uninitializedMDC.push(reinterpret_cast<GLMeshDataComponent*>(rhs));
}

void GLRenderingSystem::registerUninitializedTextureDataComponent(TextureDataComponent * rhs)
{
	GLRenderingSystemNS::m_uninitializedTDC.push(reinterpret_cast<GLTextureDataComponent*>(rhs));
}

bool GLRenderingSystem::resize()
{
	return GLRenderingSystemNS::resize();
}

bool GLRenderingSystem::reloadShader(RenderPassType renderPassType)
{
	switch (renderPassType)
	{
	case RenderPassType::Shadow:
		break;
	case RenderPassType::Opaque:
		GLEarlyZPass::reloadShader();
		GLOpaquePass::reloadShader();
		GLSSAONoisePass::reloadShader();
		GLSSAOBlurPass::reloadShader();
		break;
	case RenderPassType::Light:
		GLLightCullingPass::reloadShader();
		GLLightPass::reloadShader();
		break;
	case RenderPassType::Transparent:
		GLTransparentPass::reloadShader();
		break;
	case RenderPassType::Terrain:
		GLTerrainPass::reloadShader();
		break;
	case RenderPassType::PostProcessing:
		GLSkyPass::reloadShader();
		GLPreTAAPass::reloadShader();
		GLTAAPass::reloadShader();
		GLPostTAAPass::reloadShader();
		GLMotionBlurPass::reloadShader();
		GLBloomExtractPass::reloadShader();
		GLBloomBlurPass::reloadShader();
		GLBloomMergePass::reloadShader();
		GLBillboardPass::reloadShader();
		GLDebuggerPass::reloadShader();
		GLFinalBlendPass::reloadShader();
		break;
	default: break;
	}

	return true;
}

bool GLRenderingSystem::bakeGI()
{
	GLRenderingSystemNS::m_isBaked = false;
	return true;
}