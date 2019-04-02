#include "GLRenderingSystemUtilities.h"
#include "GLShadowRenderPass.h"
#include "../../component/GLRenderingSystemComponent.h"

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

using namespace GLRenderingSystemNS;

INNO_PRIVATE_SCOPE GLShadowRenderPass
{
	void drawAllMeshDataComponents();

	GLFrameBufferDesc DirLightShadowPassFBDesc = GLFrameBufferDesc();
	TextureDataDesc DirLightShadowPassTextureDesc = TextureDataDesc();

	GLFrameBufferDesc PointLightShadowPassFBDesc = GLFrameBufferDesc();
	TextureDataDesc PointLightShadowPassTextureDesc = TextureDataDesc();

	EntityID m_entityID;

	GLRenderPassComponent* m_DirLight_GLRPC;
	GLRenderPassComponent* m_PointLight_GLRPC;

	GLShaderProgramComponent* m_GLSPC;

	ShaderFilePaths m_shaderFilePaths = { "GL//shadowPassVertex.sf" , "", "GL//shadowPassFragment.sf" };

	GLuint m_shadowPass_uni_p;
	GLuint m_shadowPass_uni_v;
	GLuint m_shadowPass_uni_m;
}

void GLShadowRenderPass::initialize()
{
	m_entityID = InnoMath::createEntityID();

	DirLightShadowPassFBDesc.renderBufferAttachmentType = GL_DEPTH_ATTACHMENT;
	DirLightShadowPassFBDesc.renderBufferInternalFormat = GL_DEPTH_COMPONENT32;
	DirLightShadowPassFBDesc.sizeX = 2048;
	DirLightShadowPassFBDesc.sizeY = 2048;
	DirLightShadowPassFBDesc.drawColorBuffers = false;

	DirLightShadowPassTextureDesc.textureSamplerType = TextureSamplerType::SAMPLER_2D;
	DirLightShadowPassTextureDesc.textureUsageType = TextureUsageType::RENDER_TARGET;
	DirLightShadowPassTextureDesc.textureColorComponentsFormat = TextureColorComponentsFormat::DEPTH_COMPONENT;
	DirLightShadowPassTextureDesc.texturePixelDataFormat = TexturePixelDataFormat::DEPTH_COMPONENT;
	DirLightShadowPassTextureDesc.textureMinFilterMethod = TextureFilterMethod::NEAREST;
	DirLightShadowPassTextureDesc.textureMagFilterMethod = TextureFilterMethod::NEAREST;
	DirLightShadowPassTextureDesc.textureWrapMethod = TextureWrapMethod::CLAMP_TO_BORDER;
	DirLightShadowPassTextureDesc.textureWidth = DirLightShadowPassFBDesc.sizeX;
	DirLightShadowPassTextureDesc.textureHeight = DirLightShadowPassFBDesc.sizeY;
	DirLightShadowPassTextureDesc.texturePixelDataType = TexturePixelDataType::FLOAT;
	DirLightShadowPassTextureDesc.borderColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);

	m_DirLight_GLRPC = addGLRenderPassComponent(1, DirLightShadowPassFBDesc, DirLightShadowPassTextureDesc);

	PointLightShadowPassFBDesc.renderBufferAttachmentType = GL_DEPTH_ATTACHMENT;
	PointLightShadowPassFBDesc.renderBufferInternalFormat = GL_DEPTH_COMPONENT32;
	PointLightShadowPassFBDesc.sizeX = 4096;
	PointLightShadowPassFBDesc.sizeY = 4096;
	PointLightShadowPassFBDesc.drawColorBuffers = false;

	PointLightShadowPassTextureDesc.textureSamplerType = TextureSamplerType::CUBEMAP;
	PointLightShadowPassTextureDesc.textureUsageType = TextureUsageType::RENDER_TARGET;
	PointLightShadowPassTextureDesc.textureColorComponentsFormat = TextureColorComponentsFormat::DEPTH_COMPONENT;
	PointLightShadowPassTextureDesc.texturePixelDataFormat = TexturePixelDataFormat::DEPTH_COMPONENT;
	PointLightShadowPassTextureDesc.textureMinFilterMethod = TextureFilterMethod::NEAREST;
	PointLightShadowPassTextureDesc.textureMagFilterMethod = TextureFilterMethod::NEAREST;
	PointLightShadowPassTextureDesc.textureWrapMethod = TextureWrapMethod::CLAMP_TO_BORDER;
	PointLightShadowPassTextureDesc.textureWidth = PointLightShadowPassFBDesc.sizeX;
	PointLightShadowPassTextureDesc.textureHeight = PointLightShadowPassFBDesc.sizeY;
	PointLightShadowPassTextureDesc.texturePixelDataType = TexturePixelDataType::FLOAT;
	PointLightShadowPassTextureDesc.borderColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);

	m_PointLight_GLRPC = addGLRenderPassComponent(1, PointLightShadowPassFBDesc, PointLightShadowPassTextureDesc);

	// shader programs and shaders
	auto rhs = addGLShaderProgramComponent(m_entityID);
	initializeGLShaderProgramComponent(rhs, m_shaderFilePaths);

	m_shadowPass_uni_p = getUniformLocation(
		rhs->m_program,
		"uni_p");
	m_shadowPass_uni_v = getUniformLocation(
		rhs->m_program,
		"uni_v");
	m_shadowPass_uni_m = getUniformLocation(
		rhs->m_program,
		"uni_m");

	m_GLSPC = rhs;
}

void GLShadowRenderPass::drawAllMeshDataComponents()
{
	auto l_queueCopy = GLRenderingSystemComponent::get().m_opaquePassDataQueue;

	while (l_queueCopy.size() > 0)
	{
		auto l_renderPack = l_queueCopy.front();
		if (l_renderPack.meshShapeType != MeshShapeType::CUSTOM)
		{
			glFrontFace(GL_CW);
		}
		else
		{
			glFrontFace(GL_CCW);
		}
		updateUniform(
			m_shadowPass_uni_m, l_renderPack.meshUBOData.m);
		drawMesh(l_renderPack.indiceSize, l_renderPack.meshPrimitiveTopology, l_renderPack.GLMDC);
		l_queueCopy.pop();
	}
}

void GLShadowRenderPass::update()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_FRONT);

	activateShaderProgram(m_GLSPC);

	activateRenderPass(m_DirLight_GLRPC);

	auto sizeX = m_DirLight_GLRPC->m_GLFrameBufferDesc.sizeX;
	auto sizeY = m_DirLight_GLRPC->m_GLFrameBufferDesc.sizeY;

	unsigned int splitCount = 0;

	auto l_CSMDataPack = g_pCoreSystem->getVisionSystem()->getRenderingFrontend()->getCSMDataPack();

	for (unsigned int i = 0; i < 2; i++)
	{
		for (unsigned int j = 0; j < 2; j++)
		{
			glViewport(i * sizeX / 2, j * sizeY / 2, sizeX / 2, sizeY / 2);
			updateUniform(
				m_shadowPass_uni_p,
				l_CSMDataPack[splitCount].p);

			updateUniform(
				m_shadowPass_uni_v,
				l_CSMDataPack[splitCount].v);

			splitCount++;

			drawAllMeshDataComponents();
		}
	}

	//mat4 l_p = InnoMath::generatePerspectiveMatrix((90.0f / 180.0f) * PI<float>, 1.0f, 0.1f, 10.0f);
	//auto l_capturePos = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	//std::vector<mat4> l_v =
	//{
	//	InnoMath::lookAt(l_capturePos, l_capturePos + vec4(1.0f,  0.0f,  0.0f, 0.0f), vec4(0.0f, -1.0f,  0.0f, 0.0f)),
	//	InnoMath::lookAt(l_capturePos, l_capturePos + vec4(-1.0f,  0.0f,  0.0f, 0.0f), vec4(0.0f, -1.0f,  0.0f, 0.0f)),
	//	InnoMath::lookAt(l_capturePos, l_capturePos + vec4(0.0f,  1.0f,  0.0f, 0.0f), vec4(0.0f,  0.0f,  1.0f, 0.0f)),
	//	InnoMath::lookAt(l_capturePos, l_capturePos + vec4(0.0f, -1.0f,  0.0f, 0.0f), vec4(0.0f,  0.0f, -1.0f, 0.0f)),
	//	InnoMath::lookAt(l_capturePos, l_capturePos + vec4(0.0f,  0.0f,  1.0f, 0.0f), vec4(0.0f, -1.0f,  0.0f, 0.0f)),
	//	InnoMath::lookAt(l_capturePos, l_capturePos + vec4(0.0f,  0.0f, -1.0f, 0.0f), vec4(0.0f, -1.0f,  0.0f, 0.0f))
	//};

	//l_GLFBC = m_PointLight_GLRPC->m_GLFBC;

	//bindFBC(l_GLFBC);

	//updateUniform(
	//	m_shadowPass_uni_p,
	//	l_p);

	//for (unsigned int i = 0; i < 6; ++i)
	//{
	//	updateUniform(m_shadowPass_uni_v, l_v[i]);
	//	attachCubemapDepthRT(m_PointLight_GLRPC->m_GLTDCs[0], l_GLFBC, i, 0);

	//	glClear(GL_DEPTH_BUFFER_BIT);

	//	drawAllMeshDataComponents();
	//}

	//glDisable(GL_CULL_FACE);
	//glDisable(GL_DEPTH_TEST);
}

GLRenderPassComponent * GLShadowRenderPass::getGLRPC(unsigned int index)
{
	if (index == 0)
	{
		return m_DirLight_GLRPC;
	}
	else if (index == 1)
	{
		return m_PointLight_GLRPC;
	}
	else
	{
		return nullptr;
	}
}