// shadertype=glsl
#include "common/common.glsl"

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec2 inPad1;
layout(location = 3) in vec4 inNormal;
layout(location = 4) in vec4 inPad2;

struct DebugMeshData
{
	mat4 m;
	uint materialID;
	uint padding[12];
};

layout(std430, row_major, set = 1, binding = 0) buffer debugMeshSSBOBlock
{
	DebugMeshData data[];
} debugMeshSSBO;

layout(location = 0) out VS_OUT
{
	float materialID;
} vs_out;

void main()
{
	vs_out.materialID = debugMeshSSBO.data[gl_InstanceIndex].materialID;
	gl_Position = perFrameCBuffer.data.p_original * perFrameCBuffer.data.v * debugMeshSSBO.data[gl_InstanceIndex].m * inPosition;
}