// shadertype=glsl
#include "common/common.glsl"
layout(location = 0) out vec4 uni_geometryPassRT0;
layout(location = 1) out vec4 uni_geometryPassRT1;
layout(location = 2) out vec4 uni_geometryPassRT2;
layout(location = 3) out vec4 uni_geometryPassRT3;

layout(location = 0) in vec4 thefrag_WorldSpacePos;
layout(location = 1) in vec4 thefrag_ClipSpacePos_current;
layout(location = 2) in vec4 thefrag_ClipSpacePos_previous;
layout(location = 3) in vec2 thefrag_TexCoord;
layout(location = 4) in vec3 thefrag_Normal;
layout(location = 5) in float thefrag_UUID;

layout(location = 0, set = 1, binding = 0) uniform texture2D uni_normalTexture;
layout(location = 1, set = 1, binding = 1) uniform texture2D uni_albedoTexture;
layout(location = 2, set = 1, binding = 2) uniform texture2D uni_metallicTexture;
layout(location = 3, set = 1, binding = 3) uniform texture2D uni_roughnessTexture;
layout(location = 4, set = 1, binding = 4) uniform texture2D uni_aoTexture;

layout(set = 2, binding = 0) uniform sampler samplerLinear;

void main()
{
	vec3 WorldSpaceNormal;
	vec3 albedo;
	float transparency = 1.0;
	vec3 MRA;

	if ((materialCBuffer.data.textureSlotMask & 0x00000001) == 0x00000001)
	{
		// get edge vectors of the pixel triangle
		vec3 dp1 = dFdx(thefrag_WorldSpacePos.xyz);
		vec3 dp2 = dFdy(thefrag_WorldSpacePos.xyz);
		vec2 duv1 = dFdx(thefrag_TexCoord);
		vec2 duv2 = dFdy(thefrag_TexCoord);

		// solve the linear system
		vec3 N = normalize(thefrag_Normal);
		vec3 dp2perp = cross(dp2, N);
		vec3 dp1perp = cross(N, dp1);
		vec3 T = normalize(dp2perp * duv1.x + dp1perp * duv2.x);
		vec3 B = normalize(dp2perp * duv1.y + dp1perp * duv2.y);

		mat3 TBN = mat3(T, B, N);

		vec3 TangentSpaceNormal = texture(sampler2D(uni_normalTexture, samplerLinear), thefrag_TexCoord).rgb * 2.0 - 1.0;
		WorldSpaceNormal = normalize(TBN * TangentSpaceNormal);
	}
	else
	{
		WorldSpaceNormal = normalize(thefrag_Normal);
	}

	if ((materialCBuffer.data.textureSlotMask & 0x00000002) == 0x00000001)
	{
		vec4 albedoTexture = texture(sampler2D(uni_albedoTexture, samplerLinear), thefrag_TexCoord);
		transparency = albedoTexture.a;
		if (transparency < 0.1)
		{
			discard;
		}
		albedo = albedoTexture.rgb;
	}
	else
	{
		albedo = materialCBuffer.data.albedo.rgb;
	}

	if ((materialCBuffer.data.textureSlotMask & 0x00000004) == 0x00000001)
	{
		MRA.r = texture(sampler2D(uni_metallicTexture, samplerLinear), thefrag_TexCoord).r;
	}
	else
	{
		MRA.r = materialCBuffer.data.MRAT.r;
	}

	if ((materialCBuffer.data.textureSlotMask & 0x00000008) == 0x00000001)
	{
		MRA.g = texture(sampler2D(uni_roughnessTexture, samplerLinear), thefrag_TexCoord).r;
	}
	else
	{
		MRA.g = materialCBuffer.data.MRAT.g;
	}

	if ((materialCBuffer.data.textureSlotMask & 0x00000010) == 0x00000001)
	{
		MRA.b = texture(sampler2D(uni_aoTexture, samplerLinear), thefrag_TexCoord).r;
	}
	else
	{
		MRA.b = materialCBuffer.data.MRAT.b;
	}

	uni_geometryPassRT0 = vec4(thefrag_WorldSpacePos.xyz, MRA.r);
	uni_geometryPassRT1 = vec4(WorldSpaceNormal, MRA.g);
	uni_geometryPassRT2 = vec4(albedo, MRA.b);
	vec4 motionVec = (thefrag_ClipSpacePos_current / thefrag_ClipSpacePos_current.w - thefrag_ClipSpacePos_previous / thefrag_ClipSpacePos_previous.w);
	uni_geometryPassRT3 = vec4(motionVec.xy * 0.5, thefrag_UUID, float(materialCBuffer.data.materialType));
}