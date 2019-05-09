#include "GLPreTAAPass.h"

#include "GLLightPass.h"
#include "GLTransparentPass.h"
#include "GLSkyPass.h"
#include "GLTerrainPass.h"

#include "GLRenderingSystemUtilities.h"
#include "../../component/GLRenderingSystemComponent.h"

using namespace GLRenderingSystemNS;

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE GLPreTAAPass
{
	void initializeShaders();

	EntityID m_entityID;

	GLRenderPassComponent* m_GLRPC;
	GLShaderProgramComponent* m_GLSPC;
	ShaderFilePaths m_shaderFilePaths = { "GL//preTAAPass.vert/", "", "", "", "GL//preTAAPass.frag/" };
}

bool GLPreTAAPass::initialize()
{
	m_entityID = InnoMath::createEntityID();

	m_GLRPC = addGLRenderPassComponent(m_entityID, "PreTAAPassGLRPC//");
	m_GLRPC->m_renderPassDesc = GLRenderingSystemComponent::get().m_deferredRenderPassDesc;
	m_GLRPC->m_renderPassDesc.useDepthAttachment = true;
	m_GLRPC->m_renderPassDesc.useStencilAttachment = true;

	initializeGLRenderPassComponent(m_GLRPC);

	initializeShaders();

	return true;
}

void GLPreTAAPass::initializeShaders()
{
	// shader programs and shaders
	auto rhs = addGLShaderProgramComponent(m_entityID);

	initializeGLShaderProgramComponent(rhs, m_shaderFilePaths);

	m_GLSPC = rhs;
}

bool GLPreTAAPass::update()
{
	activateRenderPass(m_GLRPC);

	activateShaderProgram(m_GLSPC);

	activateTexture(
		GLLightPass::getGLRPC()->m_GLTDCs[0],
		0);
	activateTexture(
		GLSkyPass::getGLRPC()->m_GLTDCs[0],
		1);
	activateTexture(
		GLTerrainPass::getGLRPC()->m_GLTDCs[0],
		2);

	auto l_MDC = getGLMeshDataComponent(MeshShapeType::QUAD);
	drawMesh(l_MDC);

	return true;
}

bool GLPreTAAPass::resize(unsigned int newSizeX, unsigned int newSizeY)
{
	resizeGLRenderPassComponent(m_GLRPC, newSizeX, newSizeY);

	return true;
}

bool GLPreTAAPass::reloadShader()
{
	deleteShaderProgram(m_GLSPC);

	initializeGLShaderProgramComponent(m_GLSPC, m_shaderFilePaths);

	return true;
}

GLRenderPassComponent * GLPreTAAPass::getGLRPC()
{
	return m_GLRPC;
}