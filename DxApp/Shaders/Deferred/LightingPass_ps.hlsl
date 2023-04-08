// Pbr calculations ported from https://github.com/hypelive/VulkanApp/blob/master/VulkanApp/shaders/pbr.frag

#include "LightingPass.hlsli"


#define SQR_FALOFF 1

static const float kPi = 3.1415926538f;
static const float kGamma = 2.2f;
static const float kInvGamma = 1.0f / kGamma;


struct LightSource
{
	float4 color;
};

struct AmbientLightSource : LightSource { };

struct DirectionalLightSource : LightSource
{
	float4 direction;
};

struct PointLightSource : LightSource
{
	float4 position;
};

struct RectLightSource : LightSource
{
	float4 vertexPositions[4];
};

struct LightSourcesStruct
{
	static const uint kMaxDirectionalLightSourcesCount = 4;
	static const uint kMaxPointLightSourcesCount = 8;

	AmbientLightSource ambient;
	DirectionalLightSource directionalSources[kMaxDirectionalLightSourcesCount];
	PointLightSource pointLightSources[kMaxPointLightSourcesCount];

	uint directionalLightSourcesCount;
	uint pointLightSourcesCount;
};


static const float kLutSize = 64.0f;
static const float kLutScale = (kLutSize - 1.0f) / kLutSize;
static const float kLutBias = 0.5f / kLutSize;


cbuffer ConstantBuffer : register(b0)
{
	float4 CameraPosition;
	LightSourcesStruct LightSources;
};

Texture2D<float4> SurfaceColor : register(t0);
Texture2D<float4> PositionRoughness : register(t1);
Texture2D<float4> NormalMetalness : register(t2);
Texture2D<float4> FresnelIndices : register(t3);

SamplerState PointClampSampler : register(s0);
SamplerState LinearClampSampler : register(s1);


// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float3 GetFresnelReflectance(float3 n, float3 l, float3 F0)
{
	const float NdotL = dot(n, l);

	return F0 + (1 - F0) * pow(1 - max(0, NdotL), 5);
}


float GetMaskingShadowing(float3 l, float3 v, float3 m, float roughness)
{
	const float MdotV = dot(m, v);
	const float MdotL = dot(m, l);
	const float sqrMdotV = MdotV * MdotV;
	const float numerator = float(MdotV > 0 && MdotL > 0);

	return numerator / sqrt(1 + roughness * roughness * (1 - sqrMdotV) / sqrMdotV);
}

float GetNormalDistribution(float3 n, float3 m, float roughness)
{
	const float NdotM = dot(n, m);
	const float sqrAlpha = roughness * roughness;

	const float temp = 1 + NdotM * NdotM * (sqrAlpha - 1);
	return float(NdotM > 0) * sqrAlpha / (kPi * temp * temp);
}


float3 GetBrdf(float3 n, float3 v, float3 l, float3 F0, float3 rho, float roughness)
{
	const float3 h = normalize(l + v);
	const float3 F = GetFresnelReflectance(n, l, F0);
	const float G = GetMaskingShadowing(l, v, h, roughness);
	const float D = GetNormalDistribution(n, h, roughness);

	const float3 specular = F * G * D / (4 * max(1e-7f, abs(dot(n, l))) * abs(dot(n, v)));
	const float3 diffuse = (1 - F) * rho / kPi;

	return specular + diffuse;
}


void ps_main(in PixelAttributes attributes, out float4 outputColor : SV_Target)
{
	const float3 surfaceColor = SurfaceColor.Sample(PointClampSampler, attributes.uv).xyz;

	const float4 positionRoughness = PositionRoughness.Sample(PointClampSampler, attributes.uv);
	const float3 position = positionRoughness.xyz;
	const float roughness = positionRoughness.w;

	const float4 normalMetalness = NormalMetalness.Sample(PointClampSampler, attributes.uv);
	const float3 normal = normalize(normalMetalness.xyz);
	const float metalness = normalMetalness.w;

	const float3 fresnelIndices = FresnelIndices.Sample(PointClampSampler, attributes.uv).xyz;

	const float3 view = normalize(CameraPosition.xyz - position);

	const float3 F0 = lerp(fresnelIndices, surfaceColor, metalness);
	const float3 rho = lerp(surfaceColor, float3(0.0f, 0.0f, 0.0f), metalness);

	float3 radiance = float3(0.0f, 0.0f, 0.0f);
	// Ambient Light
	{
		radiance += LightSources.ambient.color.xyz * rho;
	}

	// Directional Lights
	for (uint i = 0; i < LightSources.directionalLightSourcesCount; i++)
	{
		float3 lightDirection = LightSources.directionalSources[i].direction.xyz;
		float3 brdf = GetBrdf(normal, view, lightDirection, F0, rho, roughness);

		radiance += LightSources.directionalSources[i].color.xyz * max(0, dot(lightDirection, normal)) * brdf;
	}

	// Point Lights
	for (i = 0; i < LightSources.pointLightSourcesCount; i++)
	{
		float3 pointOffset = LightSources.pointLightSources[i].position.xyz - position.xyz;
		float sqrLenght = max(1.0f, dot(pointOffset, pointOffset));
#if defined(LINEAR_FALOFF)
		float pointIntensity = 1 / sqrt(sqrLenght);
#elif defined(SQR_FALOFF)
		float pointIntensity = 1 / sqrLenght;
#endif

		float3 lightDirection = normalize(pointOffset);
		float3 brdf = GetBrdf(normal, view, lightDirection, F0, rho, roughness);

		radiance += LightSources.pointLightSources[i].color.xyz * pointIntensity * max(0, dot(lightDirection, normal)) * brdf;
	}

	outputColor = float4(pow(radiance, kInvGamma), 1.0f);
}
