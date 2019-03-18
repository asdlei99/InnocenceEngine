#pragma once
#include "../common/InnoType.h"
#include "../system/GLRenderingBackend/GLHeaders.h"

class GLShaderProgramComponent
{
public:
	GLShaderProgramComponent() {};
	~GLShaderProgramComponent() {};

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;
	EntityID m_parentEntity;

	GLuint m_program = 0;
	GLuint m_VSID = 0;
	GLuint m_GSID = 0;
	GLuint m_FSID = 0;
	GLuint m_CSID = 0;
};