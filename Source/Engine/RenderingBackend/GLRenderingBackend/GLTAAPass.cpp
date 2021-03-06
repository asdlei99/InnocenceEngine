#include "GLTAAPass.h"

#include "GLOpaquePass.h"

#include "GLRenderingBackendUtilities.h"
#include "../../Component/GLRenderingBackendComponent.h"

using namespace GLRenderingBackendNS;

#include "../../ModuleManager/IModuleManager.h"

extern IModuleManager* g_pModuleManager;

namespace GLTAAPass
{
	void initializeShaders();

	EntityID m_EntityID;

	bool m_isTAAPingPass = true;

	GLRenderPassComponent* m_PingPassGLRPC;
	GLRenderPassComponent* m_PongPassGLRPC;
	GLShaderProgramComponent* m_GLSPC;
	ShaderFilePaths m_shaderFilePaths = { "2DImageProcess.vert/", "", "", "", "TAAPass.frag/" };
}

bool GLTAAPass::initialize()
{
	m_EntityID = InnoMath::createEntityID();

	// Ping pass
	m_PingPassGLRPC = addGLRenderPassComponent(m_EntityID, "TAAPingPassGLRPC/");
	m_PingPassGLRPC->m_renderPassDesc = GLRenderingBackendComponent::get().m_deferredRenderPassDesc;
	initializeGLRenderPassComponent(m_PingPassGLRPC);

	// Pong pass
	m_PongPassGLRPC = addGLRenderPassComponent(m_EntityID, "TAAPongPassGLRPC/");
	m_PongPassGLRPC->m_renderPassDesc = GLRenderingBackendComponent::get().m_deferredRenderPassDesc;
	initializeGLRenderPassComponent(m_PongPassGLRPC);

	initializeShaders();

	return true;
}

void GLTAAPass::initializeShaders()
{
	// shader programs and shaders
	auto rhs = addGLShaderProgramComponent(m_EntityID);

	initializeGLShaderProgramComponent(rhs, m_shaderFilePaths);

	m_GLSPC = rhs;
}

bool GLTAAPass::update(GLRenderPassComponent* prePassGLRPC)
{
	GLTextureDataComponent* l_lastFrameGLTDC;
	GLRenderPassComponent* l_currentFrameGLRPC;

	if (m_isTAAPingPass)
	{
		l_lastFrameGLTDC = m_PongPassGLRPC->m_GLTDCs[0];

		l_currentFrameGLRPC = m_PingPassGLRPC;

		m_isTAAPingPass = false;
	}
	else
	{
		l_lastFrameGLTDC = m_PingPassGLRPC->m_GLTDCs[0];

		l_currentFrameGLRPC = m_PongPassGLRPC;

		m_isTAAPingPass = true;
	}

	activateShaderProgram(m_GLSPC);

	bindRenderPass(l_currentFrameGLRPC);
	cleanRenderBuffers(l_currentFrameGLRPC);

	activateTexture(prePassGLRPC->m_GLTDCs[0], 0);
	activateTexture(l_lastFrameGLTDC, 1);
	activateTexture(GLOpaquePass::getGLRPC()->m_GLTDCs[3], 2);

	auto l_MDC = getGLMeshDataComponent(MeshShapeType::QUAD);
	drawMesh(l_MDC);

	return true;
}

bool GLTAAPass::resize(uint32_t newSizeX, uint32_t newSizeY)
{
	resizeGLRenderPassComponent(m_PingPassGLRPC, newSizeX, newSizeY);
	resizeGLRenderPassComponent(m_PongPassGLRPC, newSizeX, newSizeY);

	return true;
}

bool GLTAAPass::reloadShader()
{
	deleteShaderProgram(m_GLSPC);

	initializeGLShaderProgramComponent(m_GLSPC, m_shaderFilePaths);

	return true;
}

GLRenderPassComponent * GLTAAPass::getGLRPC()
{
	if (m_isTAAPingPass)
	{
		return m_PingPassGLRPC;
	}
	else
	{
		return m_PongPassGLRPC;
	}
}