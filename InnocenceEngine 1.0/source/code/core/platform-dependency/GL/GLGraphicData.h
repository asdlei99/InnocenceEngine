#pragma once
#include "../../manager/LogManager.h"

struct VertexData
{
	glm::vec3 m_pos;
	glm::vec2 m_texCoord;
	glm::vec3 m_normal;
};

class GLMeshData
{
public:
	GLMeshData();
	~GLMeshData();

	void init();
	void update(std::vector<unsigned int>& indices);
	void shutdown();

	void addGLMeshData(std::vector<VertexData>& vertices, std::vector<unsigned int>& indices, bool calcNormals) const;

private:
	GLuint m_VAO;
	GLuint m_VBO;
	GLuint m_IBO;

	void generateData();
	void attributeArray() const;
};

class GLTextureData
{
public:
	GLTextureData();
	~GLTextureData();

	void init();
	void update();
	void shutdown();

	void loadTexture(const std::string& textureFileName) const;
	void addTextureData(int textureWidth, int textureHeight, unsigned char * textureData) const;

private:
	GLuint m_textureID;
};

class GLCubemapData
{
public:
	GLCubemapData();
	~GLCubemapData();

	void init();
	void update();
	void shutdown();

	void loadCubemap(const std::vector<std::string>& faceImagePath) const;
	void addCubemapData(unsigned int faceCount, int cubemapTextureWidth, int cubemapTextureHeight, unsigned char * cubemapTextureData) const;

private:
	GLuint m_cubemapTextureID;
};

