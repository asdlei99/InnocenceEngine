#pragma once
#include "../../common/InnoType.h"
#include "../../component/GLRenderPassComponent.h"

INNO_PRIVATE_SCOPE GLBloomBlurPass
{
	bool initialize();
	bool update(GLRenderPassComponent* prePassGLRPC);
	bool resize();
	bool reloadShader();

	GLRenderPassComponent* getGLRPC(unsigned int index);
}
