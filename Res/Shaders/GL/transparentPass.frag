// shadertype=glsl
#version 450
layout(location = 0, index = 0) out vec4 uni_transparentPassRT0;
layout(location = 0, index = 1) out vec4 uni_transparentPassRT1;

layout(location = 0) in vec4 thefrag_WorldSpacePos;
layout(location = 1) in vec2 thefrag_TexCoord;
layout(location = 2) in vec3 thefrag_Normal;

struct dirLight {
	vec4 direction;
	vec4 luminance;
	mat4 r;
};

layout(std140, row_major, binding = 0) uniform cameraUBO
{
	mat4 uni_p_camera_original;
	mat4 uni_p_camera_jittered;
	mat4 uni_r_camera;
	mat4 uni_t_camera;
	mat4 uni_r_camera_prev;
	mat4 uni_t_camera_prev;
	vec4 uni_globalPos;
	float WHRatio;
};

layout(std140, binding = 2) uniform materialUBO
{
	vec4 uni_albedo;
	vec4 uni_MRAT;
	bool uni_useNormalTexture;
	bool uni_useAlbedoTexture;
	bool uni_useMetallicTexture;
	bool uni_useRoughnessTexture;
	bool uni_useAOTexture;
};

layout(std140, row_major, binding = 3) uniform sunUBO
{
	dirLight uni_dirLight;
};

const float eps = 0.00001;
const float PI = 3.14159265359;

// Specular Fresnel Component
// ----------------------------------------------------------------------------
vec3 fr_F_Schlick(vec3 f0, float f90, float u)
{
	return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}
// Specular Visibility Component
// ----------------------------------------------------------------------------
float Unreal_GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}
// ----------------------------------------------------------------------------
float Unreal_GeometrySmith(float NdotV, float NdotL, float roughness)
{
	float ggx2 = Unreal_GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = Unreal_GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}
// Specular Distribution Component
// ----------------------------------------------------------------------------
float fr_D_GGX(float NdotH, float roughness)
{
	// remapping to Quadratic curve
	float a = roughness * roughness;
	float a2 = a * a;
	float f = (NdotH * a2 - NdotH) * NdotH + 1;
	return a2 / (pow(f, 2.0));
}
void main()
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

	vec3 WorldSpaceNormal;
	WorldSpaceNormal = normalize(TBN * vec3(0.0f, 0.0f, 1.0f));

	N = WorldSpaceNormal;
	vec3 V = normalize(uni_globalPos.xyz - thefrag_WorldSpacePos.xyz);
	vec3 L = normalize(-uni_dirLight.direction.xyz);
	vec3 H = normalize(V + L);
	float NdotV = max(dot(N, V), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	float NdotL = max(dot(N, L), 0.0);

	vec3 F0 = uni_albedo.rgb;

	// Specular BRDF
	float roughness = uni_MRAT.y;
	float f90 = 1.0;
	vec3 F = fr_F_Schlick(F0, f90, NdotV);
	float G = Unreal_GeometrySmith(NdotV, NdotL, roughness);
	float D = fr_D_GGX(NdotH, roughness);
	vec3 Frss = F * D * G / PI;

	// "Real-Time Rendering", 4th edition, pg. 624, "14.5.1 Coverage and Transmittance"
	float thickness = uni_MRAT.w;
	float d = thickness / max(NdotV, eps);

	// transmittance luminance defined as "F0/albedo"
	vec3 sigma = -(log(F0));

	vec3 Tr = exp(-sigma * d);

	// surface radiance
	vec3 Cs = Frss * uni_dirLight.luminance.xyz * uni_albedo.rgb;

	uni_transparentPassRT0 = vec4(Cs, uni_albedo.a);
	// alpha channel as the mask
	uni_transparentPassRT1 = vec4((1.0f - uni_albedo.a + Tr * uni_albedo.a), 1.0f);
}