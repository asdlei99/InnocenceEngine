#pragma once
#include "../../Common/InnoType.h"

#include "../../Component/GLMeshDataComponent.h"
#include "../../Component/GLTextureDataComponent.h"
#include "../../Component/GLMaterialDataComponent.h"
#include "../../Component/GLRenderPassComponent.h"
#include "../../Component/GLShaderProgramComponent.h"

namespace GLRenderingBackendNS
{
	bool setup();
	bool initialize();
	bool update();
	bool render();
	bool terminate();
	bool initializeComponentPool();
	bool resize();

	void loadDefaultAssets();
	bool generateGPUBuffers();

	GLMeshDataComponent* addGLMeshDataComponent();
	GLTextureDataComponent* addGLTextureDataComponent();
	GLMaterialDataComponent* addGLMaterialDataComponent();

	GLMeshDataComponent* getGLMeshDataComponent(MeshShapeType MeshShapeType);
	GLTextureDataComponent* getGLTextureDataComponent(TextureUsageType TextureUsageType);
	GLTextureDataComponent* getGLTextureDataComponent(FileExplorerIconType iconType);
	GLTextureDataComponent* getGLTextureDataComponent(WorldEditorIconType iconType);

	GLRenderPassComponent* addGLRenderPassComponent(const EntityID& parentEntity, const char* name);
	bool initializeGLRenderPassComponent(GLRenderPassComponent* GLRPC);

	bool resizeGLRenderPassComponent(GLRenderPassComponent * GLRPC, uint32_t newSizeX, uint32_t newSizeY);

	bool initializeGLMeshDataComponent(GLMeshDataComponent* rhs);
	bool initializeGLTextureDataComponent(GLTextureDataComponent* rhs);
	bool initializeGLMaterialDataComponent(GLMaterialDataComponent* rhs);

	GLShaderProgramComponent* addGLShaderProgramComponent(const EntityID& rhs);

	bool initializeGLShaderProgramComponent(GLShaderProgramComponent* rhs, const ShaderFilePaths& shaderFilePaths);

	bool deleteShaderProgram(GLShaderProgramComponent* rhs);

	GLuint getUniformLocation(GLuint shaderProgram, const std::string& uniformName);
	GLuint generateUBO(GLuint UBOSize, GLuint uniformBlockBindingPoint, const std::string& UBOName);

	void updateUBOImpl(const GLint& UBO, size_t size, const void* UBOValue);

	template<typename T>
	void updateUBO(const GLint& UBO, const T& UBOValue)
	{
		updateUBOImpl(UBO, sizeof(T), &UBOValue);
	}

	template<typename T>
	void updateUBO(const GLint& UBO, const std::vector<T>& UBOValue)
	{
		updateUBOImpl(UBO, sizeof(T) * UBOValue.size(), &UBOValue[0]);
	}

	bool bindUBO(const GLint& UBO, GLuint uniformBlockBindingPoint, uint32_t offset, uint32_t size);

	void updateUniform(const GLint uniformLocation, bool uniformValue);
	void updateUniform(const GLint uniformLocation, int32_t uniformValue);
	void updateUniform(const GLint uniformLocation, uint32_t uniformValue);
	void updateUniform(const GLint uniformLocation, float uniformValue);
	void updateUniform(const GLint uniformLocation, vec2 uniformValue);
	void updateUniform(const GLint uniformLocation, vec4 uniformValue);
	void updateUniform(const GLint uniformLocation, const mat4& mat);
	void updateUniform(const GLint uniformLocation, const std::vector<vec4>& uniformValue);
	void updateUniform(const GLint uniformLocation, const std::vector<mat4>& uniformValue);

	GLuint generateSSBO(GLuint SSBOSize, GLuint bufferBlockBindingPoint, const std::string& SSBOName);

	void updateSSBOImpl(const GLint& SSBO, size_t size, const void* SSBOValue);

	template<typename T>
	void updateSSBO(const GLint& SSBO, const T& SSBOValue)
	{
		updateSSBOImpl(SSBO, sizeof(T), &SSBOValue);
	}

	template<typename T>
	void updateSSBO(const GLint& SSBO, const std::vector<T>& SSBOValue)
	{
		updateSSBOImpl(SSBO, sizeof(T) * SSBOValue.size(), &SSBOValue[0]);
	}

	void bind2DDepthTextureForWrite(GLTextureDataComponent * GLTDC, GLRenderPassComponent * GLRPC);
	void bindCubemapDepthTextureForWrite(GLTextureDataComponent * GLTDC, GLRenderPassComponent * GLRPC, uint32_t textureIndex, uint32_t mipLevel);
	void bind2DColorTextureForWrite(GLTextureDataComponent * GLTDC, GLRenderPassComponent * GLRPC, uint32_t colorAttachmentIndex);
	void bind3DColorTextureForWrite(GLTextureDataComponent * GLTDC, GLRenderPassComponent * GLRPC, uint32_t colorAttachmentIndex, uint32_t layer);
	void bindCubemapTextureForWrite(GLTextureDataComponent * GLTDC, GLRenderPassComponent * GLRPC, uint32_t colorAttachmentIndex, uint32_t textureIndex, uint32_t mipLevel);
	void unbind2DColorTextureForWrite(GLRenderPassComponent * GLRPC, uint32_t colorAttachmentIndex);
	void unbindCubemapTextureForWrite(GLRenderPassComponent * GLRPC, uint32_t colorAttachmentIndex, uint32_t textureIndex, uint32_t mipLevel);
	void activateShaderProgram(GLShaderProgramComponent* GLShaderProgramComponent);

	void drawMesh(GLMeshDataComponent* GLMDC);
	void activateTexture(GLTextureDataComponent* GLTDC, int32_t activateIndex);

	void bindRenderPass(GLRenderPassComponent * GLRPC);
	void cleanRenderBuffers(GLRenderPassComponent * GLRPC);
	void copyDepthBuffer(GLRenderPassComponent* src, GLRenderPassComponent* dest);
	void copyStencilBuffer(GLRenderPassComponent* src, GLRenderPassComponent* dest);
	void copyColorBuffer(GLRenderPassComponent* src, uint32_t srcIndex, GLRenderPassComponent* dest, uint32_t destIndex);

	vec4 readPixel(GLRenderPassComponent* GLRPC, uint32_t colorAttachmentIndex, GLint x, GLint y);
	std::vector<vec4> readCubemapSamples(GLRenderPassComponent* GLRPC, GLTextureDataComponent* GLTDC);
}
