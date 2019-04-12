#include "GLFinalBlendPass.h"
#include "GLBloomMergePass.h"
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
	void bindUniformLocations(GLShaderProgramComponent* rhs);

	EntityID m_entityID;

	GLRenderPassComponent* m_GLRPC;
	GLShaderProgramComponent* m_GLSPC;
	ShaderFilePaths m_shaderFilePaths = { "GL//finalBlendPassVertex.sf", "", "GL//finalBlendPassFragment.sf" };

	std::vector<std::string> m_uniformNames =
	{
		"uni_basePassRT0",
		"uni_bloomPassRT0",
		"uni_billboardPassRT0",
		"uni_debuggerPassRT0",
	};
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

	bindUniformLocations(rhs);

	m_GLSPC = rhs;
}

void GLFinalBlendPass::bindUniformLocations(GLShaderProgramComponent* rhs)
{
	updateTextureUniformLocations(rhs->m_program, m_uniformNames);
}

bool GLFinalBlendPass::update(GLRenderPassComponent* prePassGLRPC)
{
	activateRenderPass(m_GLRPC);

	activateShaderProgram(m_GLSPC);

	// last pass rendering target as the mixing background
	activateTexture(
		prePassGLRPC->m_GLTDCs[0],
		0);
	// bloom pass rendering target
	activateTexture(
		GLBloomMergePass::getGLRPC()->m_GLTDCs[0],
		1);
	// billboard pass rendering target
	activateTexture(
		GLBillboardPass::getGLRPC()->m_GLTDCs[0],
		2);
	// debugger pass rendering target
	activateTexture(
		GLDebuggerPass::getGLRPC()->m_GLTDCs[0],
		3);
	// draw final pass rectangle
	auto l_MDC = g_pCoreSystem->getAssetSystem()->getMeshDataComponent(MeshShapeType::QUAD);
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

	bindUniformLocations(m_GLSPC);

	return true;
}

GLRenderPassComponent * GLFinalBlendPass::getGLRPC()
{
	return m_GLRPC;
}