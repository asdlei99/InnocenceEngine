#include "GLBRDFLUTPass.h"
#include "GLRenderingSystemUtilities.h"
#include "../../component/GLRenderingSystemComponent.h"
#include "../../component/RenderingFrontendSystemComponent.h"

using namespace GLRenderingSystemNS;

INNO_PRIVATE_SCOPE GLBRDFLUTPass
{
	EntityID m_entityID;

	GLRenderPassComponent* m_BRDFSplitSumLUTPassGLRPC;
	GLShaderProgramComponent* m_BRDFSplitSumLUTPassSPC;
	ShaderFilePaths m_BRDFSplitSumShaderFilePaths = { "GL//BRDFLUTPass.vert" , "", "GL//BRDFLUTPass.frag" };

	GLRenderPassComponent* m_BRDFMSAverageLUTPassGLRPC;
	GLShaderProgramComponent* m_BRDFMSAverageLUTPassSPC;
	ShaderFilePaths m_BRDFMSAverageShaderFilePaths = { "GL//BRDFLUTMSPass.vert" , "", "GL//BRDFLUTMSPass.frag" };
}

bool GLBRDFLUTPass::initialize()
{
	m_entityID = InnoMath::createEntityID();

	m_BRDFSplitSumLUTPassGLRPC = addGLRenderPassComponent(m_entityID, "BRDFSplitSumLUTPassGLRPC//");
	m_BRDFSplitSumLUTPassGLRPC->m_renderPassDesc = GLRenderingSystemComponent::get().m_deferredRenderPassDesc;
	m_BRDFSplitSumLUTPassGLRPC->m_renderPassDesc.RTDesc.width = 512;
	m_BRDFSplitSumLUTPassGLRPC->m_renderPassDesc.RTDesc.height = 512;
	initializeGLRenderPassComponent(m_BRDFSplitSumLUTPassGLRPC);

	m_BRDFMSAverageLUTPassGLRPC = addGLRenderPassComponent(m_entityID, "BRDFMSAverageLUTPassGLRPC//");
	m_BRDFMSAverageLUTPassGLRPC->m_renderPassDesc = GLRenderingSystemComponent::get().m_deferredRenderPassDesc;
	m_BRDFMSAverageLUTPassGLRPC->m_renderPassDesc.RTDesc.pixelDataFormat = TexturePixelDataFormat::RG;
	m_BRDFMSAverageLUTPassGLRPC->m_renderPassDesc.RTDesc.width = 512;
	m_BRDFMSAverageLUTPassGLRPC->m_renderPassDesc.RTDesc.height = 512;
	initializeGLRenderPassComponent(m_BRDFMSAverageLUTPassGLRPC);

	m_BRDFSplitSumLUTPassSPC = addGLShaderProgramComponent(m_entityID);
	initializeGLShaderProgramComponent(m_BRDFSplitSumLUTPassSPC, m_BRDFSplitSumShaderFilePaths);

	m_BRDFMSAverageLUTPassSPC = addGLShaderProgramComponent(m_entityID);
	initializeGLShaderProgramComponent(m_BRDFMSAverageLUTPassSPC, m_BRDFMSAverageShaderFilePaths);

	update();

	return true;
}

bool GLBRDFLUTPass::update()
{
	auto l_MDC = getGLMeshDataComponent(MeshShapeType::QUAD);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	// draw split-Sum LUT
	activateRenderPass(m_BRDFSplitSumLUTPassGLRPC);

	activateShaderProgram(m_BRDFSplitSumLUTPassSPC);

	drawMesh(l_MDC);

	// draw averange RsF1 LUT
	activateRenderPass(m_BRDFMSAverageLUTPassGLRPC);

	activateShaderProgram(m_BRDFMSAverageLUTPassSPC);

	activateTexture(m_BRDFSplitSumLUTPassGLRPC->m_GLTDCs[0], 0);

	drawMesh(l_MDC);

	return true;
}

bool GLBRDFLUTPass::reloadShader()
{
	deleteShaderProgram(m_BRDFSplitSumLUTPassSPC);

	initializeGLShaderProgramComponent(m_BRDFSplitSumLUTPassSPC, m_BRDFSplitSumShaderFilePaths);

	deleteShaderProgram(m_BRDFMSAverageLUTPassSPC);

	initializeGLShaderProgramComponent(m_BRDFMSAverageLUTPassSPC, m_BRDFMSAverageShaderFilePaths);

	return true;
}

GLTextureDataComponent * GLBRDFLUTPass::getBRDFSplitSumLUT()
{
	return m_BRDFSplitSumLUTPassGLRPC->m_GLTDCs[0];
}

GLTextureDataComponent * GLBRDFLUTPass::getBRDFMSAverageLUT()
{
	return m_BRDFMSAverageLUTPassGLRPC->m_GLTDCs[0];
}