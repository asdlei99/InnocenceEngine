#include "GLFinalBlendPass.h"
#include "GLBillboardPass.h"
#include "GLDebuggerPass.h"

#include "GLRenderingSystemUtilities.h"
#include "../../component/GLRenderingSystemComponent.h"

using namespace GLRenderingSystemNS;

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE GLFinalBlendPass
{
	void initializeShaders();

	EntityID m_entityID;

	GLRenderPassComponent* m_GLRPC;
	GLShaderProgramComponent* m_GLSPC;
	ShaderFilePaths m_shaderFilePaths = { "GL//finalBlendPass.vert", "", "GL//finalBlendPass.frag" };
}

bool GLFinalBlendPass::initialize()
{
	m_entityID = InnoMath::createEntityID();

	m_GLRPC = addGLRenderPassComponent(1, GLRenderingSystemComponent::get().deferredPassFBDesc, GLRenderingSystemComponent::get().deferredPassTextureDesc);

	initializeShaders();

	return true;
}

void GLFinalBlendPass::initializeShaders()
{
	// shader programs and shaders
	auto rhs = addGLShaderProgramComponent(m_entityID);

	initializeGLShaderProgramComponent(rhs, m_shaderFilePaths);

	m_GLSPC = rhs;
}

bool GLFinalBlendPass::update(GLRenderPassComponent* prePassGLRPC)
{
	activateRenderPass(m_GLRPC);

	activateShaderProgram(m_GLSPC);

	// last pass rendering target as the mixing background
	activateTexture(
		prePassGLRPC->m_GLTDCs[0],
		0);
	// billboard pass rendering target
	activateTexture(
		GLBillboardPass::getGLRPC()->m_GLTDCs[0],
		1);
	// debugger pass rendering target
	activateTexture(
		GLDebuggerPass::getGLRPC()->m_GLTDCs[0],
		2);
	// draw final pass rectangle
	auto l_MDC = getGLMeshDataComponent(MeshShapeType::QUAD);
	drawMesh(l_MDC);

	// draw again for game build
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	drawMesh(l_MDC);

	return true;
}

bool GLFinalBlendPass::resize(unsigned int newSizeX, unsigned int newSizeY)
{
	resizeGLRenderPassComponent(m_GLRPC, newSizeX, newSizeY);

	return true;
}

bool GLFinalBlendPass::reloadShader()
{
	deleteShaderProgram(m_GLSPC);

	initializeGLShaderProgramComponent(m_GLSPC, m_shaderFilePaths);

	return true;
}

GLRenderPassComponent * GLFinalBlendPass::getGLRPC()
{
	return m_GLRPC;
}