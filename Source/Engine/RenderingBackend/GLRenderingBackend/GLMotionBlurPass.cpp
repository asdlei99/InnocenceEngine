#include "GLMotionBlurPass.h"
#include "GLOpaquePass.h"

#include "GLRenderingBackendUtilities.h"
#include "../../Component/GLRenderingBackendComponent.h"

using namespace GLRenderingBackendNS;

#include "../../ModuleManager/IModuleManager.h"

extern IModuleManager* g_pModuleManager;

INNO_PRIVATE_SCOPE GLMotionBlurPass
{
	void initializeShaders();

	EntityID m_entityID;

	GLRenderPassComponent* m_GLRPC;
	GLShaderProgramComponent* m_GLSPC;
	ShaderFilePaths m_shaderFilePaths = { "motionBlurPass.vert/", "", "", "", "motionBlurPass.frag/" };
}

bool GLMotionBlurPass::initialize()
{
	m_entityID = InnoMath::createEntityID();

	m_GLRPC = addGLRenderPassComponent(m_entityID, "MotionBlurPassGLRPC/");
	m_GLRPC->m_renderPassDesc = GLRenderingBackendComponent::get().m_deferredRenderPassDesc;
	initializeGLRenderPassComponent(m_GLRPC);

	initializeShaders();

	return true;
}

void GLMotionBlurPass::initializeShaders()
{
	// shader programs and shaders
	auto rhs = addGLShaderProgramComponent(m_entityID);

	initializeGLShaderProgramComponent(rhs, m_shaderFilePaths);

	m_GLSPC = rhs;
}

bool GLMotionBlurPass::update(GLRenderPassComponent* prePassGLRPC)
{
	bindRenderPass(m_GLRPC);
	cleanRenderBuffers(m_GLRPC);

	activateShaderProgram(m_GLSPC);

	activateTexture(
		GLOpaquePass::getGLRPC()->m_GLTDCs[3],
		0);
	activateTexture(
		prePassGLRPC->m_GLTDCs[0],
		1);

	auto l_MDC = getGLMeshDataComponent(MeshShapeType::QUAD);
	drawMesh(l_MDC);

	return true;
}

bool GLMotionBlurPass::resize(unsigned int newSizeX, unsigned int newSizeY)
{
	resizeGLRenderPassComponent(m_GLRPC, newSizeX, newSizeY);

	return true;
}

bool GLMotionBlurPass::reloadShader()
{
	deleteShaderProgram(m_GLSPC);

	initializeGLShaderProgramComponent(m_GLSPC, m_shaderFilePaths);

	return true;
}

GLRenderPassComponent * GLMotionBlurPass::getGLRPC()
{
	return m_GLRPC;
}