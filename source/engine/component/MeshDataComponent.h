#pragma once
#include "BaseComponent.h"

class MeshDataComponent : public BaseComponent
{
public:
	MeshDataComponent() { m_meshID = std::rand(); };
	~MeshDataComponent() {};

	meshID m_meshID;
	meshType m_meshType;
	std::vector<Vertex> m_vertices;
	std::vector<unsigned int> m_indices;

	meshDrawMethod m_meshDrawMethod;
	bool m_calculateNormals;
	bool m_calculateTangents;

	objectStatus m_objectStatus = objectStatus::SHUTDOWN;
};

