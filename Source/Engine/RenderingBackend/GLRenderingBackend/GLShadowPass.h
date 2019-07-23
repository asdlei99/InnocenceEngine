#pragma once
#include "../../Common/InnoType.h"
#include "../../Component/GLRenderPassComponent.h"

namespace GLShadowPass
{
	void initialize();

	void update();
	bool reloadShader();

	GLRenderPassComponent* getGLRPC(unsigned int index);
}