// Pbr calculations ported from https://github.com/hypelive/VulkanApp/blob/master/VulkanApp/shaders/pbr.frag

#include "LightingPass.hlsli"


#define SQR_FALOFF 1


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

// TODO struct AreaLightSource : LightSource { float4 vertices[4]; }


static const uint kMaxDirectionalLightSourcesCount = 4;
static const uint kMaxPointLightSourcesCount = 8;

struct LightSourcesStruct
{
	AmbientLightSource ambient;
	DirectionalLightSource directionalSources[kMaxDirectionalLightSourcesCount];
	PointLightSource pointLightSources[kMaxPointLightSourcesCount];

	uint directionalLightSourcesCount;
	uint pointLightSourcesCount;
};


cbuffer ConstantBuffer : register(b0)
{
	float4 CameraPosition;
	LightSourcesStruct LightSources;
};


Texture2D<float4> Albedo : register(t0);
Texture2D<float4> Position : register(t1);
Texture2D<float4> Normal : register(t2);

SamplerState PointClampSampler : register(s0);


static const float kPi = 3.1415926538f;
static const float3 kF0 = float3(0.6f, 0.7f, 0.8f); // F0 - almost specular color, but no...
static const float kAlpha = 0.1f; // Roughtness


float3 GetFresnelReflectance(float3 n, float3 l)
{
	float NdotL = dot(n, l);

	return kF0 + (1 - kF0) * pow(1 - max(0, NdotL), 5);
}


float GetMaskingShadowing(float3 l, float3 v, float3 m)
{
	float MdotV = dot(m, v);
	float MdotL = dot(m, l);
	float sqrMdotV = MdotV * MdotV;
	float numerator = float(MdotV > 0 && MdotL > 0);

	return numerator / sqrt(1 + kAlpha * kAlpha * (1 - sqrMdotV) / sqrMdotV);
}

float GetNormalDistribution(float3 n, float3 m)
{
	float NdotM = dot(n, m);
	float sqrAlpha = kAlpha * kAlpha;

	float temp = 1 + NdotM * NdotM * (sqrAlpha - 1);
	return float(NdotM > 0) * sqrAlpha / (kPi * temp * temp);
}


float3 GetBrdf(float3 n, float3 v, float3 l, float3 rho)
{
	float3 h = normalize(l + v);
	float3 F = GetFresnelReflectance(n, l);
	float G = GetMaskingShadowing(l, v, h);
	float D = GetNormalDistribution(n, h);

	float3 specular = F * G * D / (4 * max(0.00001f, abs(dot(n, l))) * abs(dot(n, v)));
	float3 diffuse = (1 - F) * rho / kPi;

	return specular + diffuse;
}


void ps_main(in PixelAttributes attributes, out float4 outputColor : SV_Target)
{
	float3 albedo = Albedo.Sample(PointClampSampler, attributes.uv).xyz;
	float3 position = Position.Sample(PointClampSampler, attributes.uv).xyz;
	float3 normal = normalize(Normal.Sample(PointClampSampler, attributes.uv)).xyz;

	float3 view = normalize(CameraPosition.xyz - position);

	float3 radiance = float3(0.0f, 0.0f, 0.0f);
	radiance += LightSources.ambient.color.xyz;

	for (uint i = 0; i < LightSources.directionalLightSourcesCount; i++)
	{
		float3 lightDirection = LightSources.directionalSources[i].direction.xyz;
		float3 brdf = GetBrdf(normal, view, lightDirection, albedo);

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
		float3 brdf = GetBrdf(normal, view, lightDirection, albedo);

		radiance += LightSources.pointLightSources[i].color.xyz * pointIntensity * max(0, dot(lightDirection, normal)) * brdf;
	}

	outputColor = float4(radiance, 1.0f);
}
