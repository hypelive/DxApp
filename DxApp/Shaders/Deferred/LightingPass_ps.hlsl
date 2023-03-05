// Pbr calculations ported from https://github.com/hypelive/VulkanApp/blob/master/VulkanApp/shaders/pbr.frag

#include "LightingPass.hlsli"


#define SQR_FALOFF 1

static const float kPi = 3.1415926538f;


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

//struct AreaLightSource : LightSource
//{
//	float4 vertexPositions[4];
//};

struct LightSourcesStruct
{
	static const uint kMaxDirectionalLightSourcesCount = 4;
	static const uint kMaxPointLightSourcesCount = 8;
	//static const uint kMaxAreaLightSourcesCount = 4;

	AmbientLightSource ambient;
	DirectionalLightSource directionalSources[kMaxDirectionalLightSourcesCount];
	PointLightSource pointLightSources[kMaxPointLightSourcesCount];
	//AreaLightSource areaLightSources[kMaxAreaLightSourcesCount];

	uint directionalLightSourcesCount;
	uint pointLightSourcesCount;
	//uint areaLightSourcesCount;
};


cbuffer ConstantBuffer : register(b0)
{
	float4 CameraPosition;
	LightSourcesStruct LightSources;
};

Texture2D<float4> SurfaceColor : register(t0);
Texture2D<float4> PositionRoughness : register(t1);
Texture2D<float4> NormalMetalness : register(t2);
Texture2D<float4> FresnelIndices : register(t3);

Texture2D<float4> Ltc1 : register(t4);
Texture2D<float4> Ltc2 : register(t5);

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


float3 GetBrdf(float3 n, float3 v, float3 l, float3 surfaceColor, float metalness, float roughness, float3 fresnelIndices)
{
	const float3 F0 = lerp(fresnelIndices, surfaceColor, metalness);
	const float3 rho = lerp(surfaceColor, float3(0.0f, 0.0f, 0.0f), metalness);

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

	float3 radiance = float3(0.0f, 0.0f, 0.0f);
	radiance += LightSources.ambient.color.xyz;

	for (uint i = 0; i < LightSources.directionalLightSourcesCount; i++)
	{
		float3 lightDirection = LightSources.directionalSources[i].direction.xyz;
		float3 brdf = GetBrdf(normal, view, lightDirection, surfaceColor, metalness, roughness, fresnelIndices);

		radiance += LightSources.directionalSources[i].color.xyz * max(0, dot(lightDirection, normal)) * brdf;
	}

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
		float3 brdf = GetBrdf(normal, view, lightDirection, surfaceColor, metalness, roughness, fresnelIndices);

		radiance += LightSources.pointLightSources[i].color.xyz * pointIntensity * max(0, dot(lightDirection, normal)) * brdf;
	}

	const float4 ltc1 = Ltc1.Sample(LinearClampSampler, float2(0, 0));
	outputColor = float4(ltc1.xyz, 1.0f);
	return;

	//for (i = 0; i < LightSources.areaLightSourcesCount; i++)
	//{
	//	 //TODO
	//}

	outputColor = float4(radiance, 1.0f);
}
