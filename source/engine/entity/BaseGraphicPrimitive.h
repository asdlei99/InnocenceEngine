#pragma once
#include "interface/IObject.hpp"
#include "innoMath.h"

enum class visiblilityType { INVISIBLE, BILLBOARD, STATIC_MESH, SKYBOX, GLASSWARE };
enum class meshType { QUAD, CUBE, SPHERE, CUSTOM };
enum class meshDrawMethod { TRIANGLE, TRIANGLE_STRIP };
enum class textureType { INVISIBLE, NORMAL, ALBEDO, METALLIC, ROUGHNESS, AMBIENT_OCCLUSION, CUBEMAP, CUBEMAP_HDR, EQUIRETANGULAR };
enum class textureWrapMethod { CLAMP_TO_EDGE, REPEAT };
enum class textureFilterMethod { NEAREST, LINEAR, LINEAR_MIPMAP_LINEAR };

class IMeshRawData
{
public:
	IMeshRawData() {};
	virtual ~IMeshRawData() {};

	virtual int getNumVertices() const = 0;
	virtual int getNumFaces() const = 0;
	virtual int getNumIndicesInFace(int faceIndex) const = 0;
	virtual vec3 getVertices(unsigned int index) const = 0;
	virtual vec2 getTextureCoords(unsigned int index) const = 0;
	virtual vec3 getNormals(unsigned int index) const = 0;
	virtual int getIndices(int faceIndex, int index) const = 0;
};

class IScene
{
public:
	IScene() {};
	virtual ~IScene() {};
};

typedef unsigned long int textureID;
typedef unsigned long int meshID;
typedef std::pair<textureType, textureID> texturePair;
typedef std::unordered_map<textureType, textureID> textureMap;
typedef std::pair<meshID, textureMap> modelPair;
typedef std::unordered_map<meshID, textureMap> modelMap;

typedef std::pair<textureType, std::string> textureFileNamePair;
typedef std::unordered_map<textureType, std::string> textureFileNameMap;
typedef std::pair<IMeshRawData*, textureFileNameMap> modelPointerPair;
typedef std::unordered_map<IMeshRawData*, textureFileNameMap> modelPointerMap;


class BaseMesh : public IObject
{
public:
	BaseMesh() { m_meshID = std::rand(); };
	virtual ~BaseMesh() {};

	void setup() override;
	void setup(meshDrawMethod meshDrawMethod, bool calculateNormals, bool calculateTangents);
	const objectStatus& getStatus() const override;
	meshID getMeshID();

	void addVertices(const Vertex& Vertex);
	void addVertices(const vec3 & pos, const vec2 & texCoord, const vec3 & normal);
	void addVertices(double pos_x, double pos_y, double pos_z, double texCoord_x, double texCoord_y, double normal_x, double normal_y, double normal_z);
	void addIndices(unsigned int index);

	void addUnitCube();
	void addUnitSphere();
	void addUnitQuad();

protected:
	objectStatus m_objectStatus = objectStatus::SHUTDOWN;
	meshID m_meshID;

	std::vector<Vertex> m_vertices;
	std::vector<unsigned int> m_indices;

	meshDrawMethod m_meshDrawMethod;
	bool m_calculateNormals;
	bool m_calculateTangents;
};

class BaseTexture : public IObject
{
public:
	BaseTexture() { m_textureID = std::rand(); };
	virtual ~BaseTexture() {};

	void setup() override;
	void setup(textureType textureType, textureWrapMethod textureWrapMethod, int textureFormat, int textureWidth, int textureHeight, const std::vector<void *>& textureData, bool generateMipMap);
	const objectStatus& getStatus() const override;
	textureID getTextureID();

protected:
	objectStatus m_objectStatus = objectStatus::SHUTDOWN;
	textureID m_textureID;
	textureType m_textureType;
	textureWrapMethod m_textureWrapMethod;
	int m_textureFormat;
	int m_textureWidth;
	int m_textureHeight;	
	std::vector<void *> m_textureData;
	bool m_generateMipMap;
};

class BaseFrameBuffer : public IObject
{
public:
	BaseFrameBuffer() {};
	virtual ~BaseFrameBuffer() {};

	void setup() override;
	void setup(vec2 renderBufferStorageResolution, bool isDeferPass, unsigned int renderTargetTextureNumber);
	const objectStatus& getStatus() const override;

protected:
	objectStatus m_objectStatus = objectStatus::SHUTDOWN;
	vec2 m_renderBufferStorageResolution;
	bool m_isDeferPass = false;
	unsigned int m_renderTargetTextureNumber = 0;
};
