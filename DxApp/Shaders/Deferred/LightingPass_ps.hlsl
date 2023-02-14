#include "LightingPass.hlsli"


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


void ps_main(in PixelAttributes attributes, out float4 outputColor : SV_Target)
{
	float4 albedo = Albedo.Sample(PointClampSampler, attributes.uv);
	float4 position = Position.Sample(PointClampSampler, attributes.uv);
	float4 normal = normalize(Normal.Sample(PointClampSampler, attributes.uv));

	// TODO

	outputColor = albedo;
}
