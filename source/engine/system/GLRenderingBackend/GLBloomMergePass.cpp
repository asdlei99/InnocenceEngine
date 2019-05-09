#include "GLBloomMergePass.h"
#include "GLBloomExtractPass.h"
#include "GLPreTAAPass.h"

#include "GLRenderingSystemUtilities.h"
#include "../../component/GLRenderingSystemComponent.h"

using namespace GLRenderingSystemNS;

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE GLBloomMergePass
{
	void initializeShaders();

	EntityID m_entityID;

	GLRenderPassComponent* m_GLRPC;
	GLShaderProgramComponent* m_GLSPC;
	ShaderFilePaths m_shaderFilePaths = { "GL//bloomMergePass.vert", "", "GL//bloomMergePass.frag" };
}

bool GLBloomMergePass::initialize()
{
	m_entityID = InnoMath::createEntityID();

	auto l_renderPassDesc = GLRenderingSystemComponent::get().m_deferredRenderPassDesc;
	l_renderPassDesc.RTDesc.magFilterMethod = TextureFilterMethod::LINEAR;
	l_renderPassDesc.RTDesc.minFilterMethod = TextureFilterMethod::LINEAR_MIPMAP_LINEAR;

	m_GLRPC = addGLRenderPassComponent(m_entityID, "BloomMergePassGLRPC//");
	m_GLRPC->m_renderPassDesc = l_renderPassDesc;
	initializeGLRenderPassComponent(m_GLRPC);

	initializeShaders();

	return true;
}

void GLBloomMergePass::initializeShaders()
{
	// shader programs and shaders
	auto rhs = addGLShaderProgramComponent(m_entityID);

	initializeGLShaderProgramComponent(rhs, m_shaderFilePaths);

	m_GLSPC = rhs;
}

bool GLBloomMergePass::update()
{
	activateRenderPass(m_GLRPC);

	activateShaderProgram(m_GLSPC);

	activateTexture(
		GLBloomExtractPass::getGLRPC(0)->m_GLTDCs[0],
		0);
	activateTexture(
		GLBloomExtractPass::getGLRPC(1)->m_GLTDCs[0],
		1);
	activateTexture(
		GLBloomExtractPass::getGLRPC(2)->m_GLTDCs[0],
		2);
	activateTexture(
		GLBloomExtractPass::getGLRPC(3)->m_GLTDCs[0],
		3);
	activateTexture(
		GLPreTAAPass::getGLRPC()->m_GLTDCs[0],
		4);
	auto l_MDC = getGLMeshDataComponent(MeshShapeType::QUAD);
	drawMesh(l_MDC);

	return true;
}

bool GLBloomMergePass::resize(unsigned int newSizeX, unsigned int newSizeY)
{
	resizeGLRenderPassComponent(m_GLRPC, newSizeX, newSizeY);

	return true;
}

bool GLBloomMergePass::reloadShader()
{
	deleteShaderProgram(m_GLSPC);

	initializeGLShaderProgramComponent(m_GLSPC, m_shaderFilePaths);

	return true;
}

GLRenderPassComponent * GLBloomMergePass::getGLRPC()
{
	return m_GLRPC;
}