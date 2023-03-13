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

struct AreaLightSource : LightSource
{
	float4 vertexPositions[4];
};

struct LightSourcesStruct
{
	static const uint kMaxDirectionalLightSourcesCount = 4;
	static const uint kMaxPointLightSourcesCount = 8;
	static const uint kMaxAreaLightSourcesCount = 4;

	AmbientLightSource ambient;
	DirectionalLightSource directionalSources[kMaxDirectionalLightSourcesCount];
	PointLightSource pointLightSources[kMaxPointLightSourcesCount];
	AreaLightSource areaLightSources[kMaxAreaLightSourcesCount];

	uint directionalLightSourcesCount;
	uint pointLightSourcesCount;
	uint areaLightSourcesCount;
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

Texture2D<float4> MInversedCoefficients : register(t4);
Texture2D<float2> FresnelMaskingShadowing : register(t5);
Texture2D<float> HorizonClippingCoefficients : register(t6);

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


float3 IntegrateEdgeVec(float3 v1, float3 v2)
{
	// Using built-in acos() function will result flaws
	// Using fitting result for calculating acos()
	float x = dot(v1, v2);
	float y = abs(x);

	float a = 0.8543985f + (0.4965155f + 0.0145206f * y) * y;
	float b = 3.4175940f + (4.1616724f + y) * y;
	float v = a / b;

	float theta_sintheta = (x > 0.0f) ? v : 0.5f * rsqrt(max(1.0f - x * x, 1e-7f)) - v;

	return cross(v1, v2) * theta_sintheta;
}


float3 LtcEvaluate(float3 n, float3 v, float3 position, float3x3 MInversed, float4 points[4])
{
	// construct orthonormal basis around N
	float3 T1, T2;
	T1 = normalize(v - n * dot(v, n));
	T2 = cross(n, T1);

	// rotate area light in (T1, T2, N) basis
	MInversed = mul(MInversed, transpose(float3x3(T1, T2, n)));

	// polygon (allocate 4 vertices for clipping)
	float3 L[4];
	// transform polygon from LTC back to origin Do (cosine weighted)
	L[0] = mul(MInversed, points[0].xyz - position);
	L[1] = mul(MInversed, points[1].xyz - position);
	L[2] = mul(MInversed, points[2].xyz - position);
	L[3] = mul(MInversed, points[3].xyz - position);

	// cos weighted space
	L[0] = normalize(L[0]);
	L[1] = normalize(L[1]);
	L[2] = normalize(L[2]);
	L[3] = normalize(L[3]);

	// integrate, Stoke's theorem
	float3 vsum = float3(0.0f, 0.0f, 0.0f);
	vsum += IntegrateEdgeVec(L[0], L[1]);
	vsum += IntegrateEdgeVec(L[1], L[2]);
	vsum += IntegrateEdgeVec(L[2], L[3]);
	vsum += IntegrateEdgeVec(L[3], L[0]);

	// form factor of the polygon in direction vsum
	float formFactor = length(vsum);
	float z = vsum.z / formFactor;
	float2 horizonClippingUv = float2(z * 0.5f + 0.5f, formFactor); // range [0, 1]
	horizonClippingUv = horizonClippingUv * kLutScale + kLutBias;

	// Fetch the form factor for horizon clipping
	float coefficient = HorizonClippingCoefficients.Sample(LinearClampSampler, horizonClippingUv);
	float sum = formFactor * coefficient;

	return sum.xxx;
}


// GGX BRDF
// fresnelTerm: shadowedF90 (F90 normally it should be 1.0)
// maskingShadowingTerm: Smith function for Geometric Attenuation Term, it is dot(V or L, H).
float3 GetBrdfArea(float3 n, float3 v, float3 position, float3x3 MInversed, float4 points[4],
	float3 F0, float3 rho, float fresnelTerm, float maskingShadowingTerm)
{
	static const float3x3 kIdentity = { 
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f
	};

	float3 direction0 = points[0].xyz - position;
	float3 lightNormal = cross(points[1].xyz - points[0].xyz, points[3].xyz - points[0].xyz); // TODO Is it ccw or cw?
	bool isForward = (dot(direction0, lightNormal) <= 0.0f);
	if (isForward)
	{
		float3 specular = LtcEvaluate(n, v, position, MInversed, points);
		specular *= F0 * fresnelTerm + (1.0f - F0) * maskingShadowingTerm;

		const float3 diffuse = LtcEvaluate(n, v, position, kIdentity, points);

		return specular + rho * diffuse;
	}
	else
	{
		return float3(0.0f, 0.0f, 0.0f);
	}
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

	// Area Lights
	// https://learnopengl.com/Guest-Articles/2022/Area-Lights
	// https://advances.realtimerendering.com/s2016/s2016_ltc_rnd.pdf
	{
		const float NdotV = max(0.0f, dot(normal, view));
		float2 LtcUv = float2(roughness, sqrt(1.0f - NdotV));
		LtcUv = LtcUv * kLutScale + kLutBias;

		const float4 MInversedCoefs = MInversedCoefficients.Sample(LinearClampSampler, LtcUv);
		const float2 fresnelMaskingShadowing = FresnelMaskingShadowing.Sample(LinearClampSampler, LtcUv);

		float3x3 MInversed = float3x3(
			float3(MInversedCoefs.x, 0, MInversedCoefs.y),
			float3(0, 1, 0),
			float3(MInversedCoefs.z, 0, MInversedCoefs.w)
			);

		for (i = 0; i < LightSources.areaLightSourcesCount; i++)
		{
			const float3 brdf = GetBrdfArea(normal, view, position, MInversed, LightSources.areaLightSources[i].vertexPositions,
				F0, rho, fresnelMaskingShadowing.x, fresnelMaskingShadowing.y);
			radiance += LightSources.areaLightSources[i].color.xyz * brdf;
		}
	}

	outputColor = float4(pow(radiance, kInvGamma), 1.0f);
}
