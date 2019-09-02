// shadertype=glsl
#include "common/common.glsl"

layout(location = 0, index = 0) out vec4 uni_transparentPassRT0;
layout(location = 0, index = 1) out vec4 uni_transparentPassRT1;

layout(location = 0) in vec4 posWS;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec3 Normal;
layout(location = 0, binding = 0) uniform sampler2D uni_brdfLUT;
layout(location = 1, binding = 1) uniform sampler2D uni_brdfMSLUT;

#include "common/BSDF.glsl"

void main()
{
	vec3 N = normalize(Normal);

	vec3 V = normalize(cameraUBO.globalPos.xyz - posWS.xyz);
	vec3 L = normalize(-sunUBO.data.direction.xyz);
	vec3 H = normalize(V + L);
	float NdotV = max(dot(N, V), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	float NdotL = max(dot(N, L), 0.0);

	vec3 F0 = materialUBO.Albedo.rgb;

	// Specular BRDF
	float roughness = materialUBO.MRAT.y;
	float f90 = 1.0;
	vec3 F = F_Schlick(F0, f90, NdotV);
	float G = Unreal_GeometrySmith(NdotV, NdotL, roughness);
	float D = D_GGX(NdotH, roughness);
	vec3 Frss = F * D * G / PI;

	// "Real-Time Rendering", 4th edition, pg. 624, "14.5.1 Coverage and Transmittance"
	float thickness = materialUBO.MRAT.w;
	float d = thickness / max(NdotV, eps);

	// transmittance luminance defined as "F0/albedo"
	vec3 sigma = -(log(F0));

	vec3 Tr = exp(-sigma * d);

	// surface radiance
	vec3 Cs = Frss * sunUBO.data.illuminance.xyz * materialUBO.Albedo.rgb;

	uni_transparentPassRT0 = vec4(Cs, materialUBO.Albedo.a);
	// alpha channel as the mask
	uni_transparentPassRT1 = vec4((1.0f - materialUBO.Albedo.a + Tr * materialUBO.Albedo.a), 1.0f);
}