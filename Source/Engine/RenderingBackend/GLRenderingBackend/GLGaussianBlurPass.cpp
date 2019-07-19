#include "GLGaussianBlurPass.h"

#include "GLRenderingBackendUtilities.h"
#include "../../Component/GLRenderingBackendComponent.h"

using namespace GLRenderingBackendNS;

#include "../../ModuleManager/IModuleManager.h"

extern IModuleManager* g_pModuleManager;

INNO_PRIVATE_SCOPE GLGaussianBlurPass
{
	void initializeShaders();

	EntityID m_entityID;

	GLRenderPassComponent* m_PingPassGLRPC;
	GLRenderPassComponent* m_PongPassGLRPC;
	GLShaderProgramComponent* m_GLSPC;
	ShaderFilePaths m_shaderFilePaths = { "gaussianBlurPass.vert/", "", "", "", "gaussianBlurPass.frag/" };
}

bool GLGaussianBlurPass::initialize()
{
	m_entityID = InnoMath::createEntityID();

	//Ping pass
	m_PingPassGLRPC = addGLRenderPassComponent(m_entityID, "GaussianBlurPingPassGLRPC/");
	m_PingPassGLRPC->m_renderPassDesc = GLRenderingBackendComponent::get().m_deferredRenderPassDesc;
	initializeGLRenderPassComponent(m_PingPassGLRPC);

	//Pong pass
	m_PongPassGLRPC = addGLRenderPassComponent(m_entityID, "GaussianBlurPongPassGLRPC/");
	m_PongPassGLRPC->m_renderPassDesc = GLRenderingBackendComponent::get().m_deferredRenderPassDesc;
	initializeGLRenderPassComponent(m_PongPassGLRPC);

	initializeShaders();

	return true;
}

void GLGaussianBlurPass::initializeShaders()
{
	// shader programs and shaders
	auto rhs = addGLShaderProgramComponent(m_entityID);

	initializeGLShaderProgramComponent(rhs, m_shaderFilePaths);

	m_GLSPC = rhs;
}

bool GLGaussianBlurPass::update(GLRenderPassComponent* prePassGLRPC, unsigned int RTIndex, unsigned int kernel)
{
	GLTextureDataComponent* l_currentFrameGaussianBlurGLTDC = m_PingPassGLRPC->m_GLTDCs[0];
	GLTextureDataComponent* l_lastFrameGaussianBlurGLTDC = m_PongPassGLRPC->m_GLTDCs[0];

	activateShaderProgram(m_GLSPC);

	bool l_isPing = true;
	bool l_isFirstIteration = true;

	auto l_MDC = getGLMeshDataComponent(MeshShapeType::QUAD);

	updateUniform(1, kernel);

	for (size_t i = 0; i < 5; i++)
	{
		if (l_isPing)
		{
			l_currentFrameGaussianBlurGLTDC = m_PingPassGLRPC->m_GLTDCs[0];
			l_lastFrameGaussianBlurGLTDC = m_PongPassGLRPC->m_GLTDCs[0];

			bindRenderPass(m_PingPassGLRPC);
			cleanRenderBuffers(m_PingPassGLRPC);

			updateUniform(0, true);

			if (l_isFirstIteration)
			{
				activateTexture(prePassGLRPC->m_GLTDCs[RTIndex], 0);
				l_isFirstIteration = false;
			}
			else
			{
				activateTexture(l_lastFrameGaussianBlurGLTDC, 0);
			}

			drawMesh(l_MDC);

			l_isPing = false;
		}
		else
		{
			l_currentFrameGaussianBlurGLTDC = m_PongPassGLRPC->m_GLTDCs[0];
			l_lastFrameGaussianBlurGLTDC = m_PingPassGLRPC->m_GLTDCs[0];

			bindRenderPass(m_PongPassGLRPC);
			cleanRenderBuffers(m_PongPassGLRPC);

			updateUniform(0, false);

			activateTexture(l_lastFrameGaussianBlurGLTDC, 0);

			drawMesh(l_MDC);

			l_isPing = true;
		}
	}

	// store result on another channel
	copyColorBuffer(m_PongPassGLRPC, 0, prePassGLRPC, RTIndex);

	return true;
}

bool GLGaussianBlurPass::resize(unsigned int newSizeX, unsigned int newSizeY)
{
	resizeGLRenderPassComponent(m_PingPassGLRPC, newSizeX, newSizeY);
	resizeGLRenderPassComponent(m_PongPassGLRPC, newSizeX, newSizeY);

	return true;
}

bool GLGaussianBlurPass::reloadShader()
{
	deleteShaderProgram(m_GLSPC);

	initializeGLShaderProgramComponent(m_GLSPC, m_shaderFilePaths);

	return true;
}

GLRenderPassComponent * GLGaussianBlurPass::getGLRPC(unsigned int index)
{
	if (index == 0)
	{
		return m_PingPassGLRPC;
	}
	else if (index == 1)
	{
		return m_PongPassGLRPC;
	}
	else
	{
		return nullptr;
	}
}