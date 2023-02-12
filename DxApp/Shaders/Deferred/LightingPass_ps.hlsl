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

struct LightSources
{
	AmbientLightSource ambient;
	DirectionalLightSource directionalSources[kMaxDirectionalLightSourcesCount];
	PointLightSource pointLightSources[kMaxPointLightSourcesCount];

	uint directionalLightSourcesCount;
	uint pointLightSourcesCount;
};


cbuffer ConstantBuffer : register(b0)
{
	LightSources lightSources;
}


// Need point clamp static sampler
void ps_main(in PixelAttributes attributes, out float4 outputColor : SV_Target)
{
	outputColor = float4(1.0f, 0.0f, 1.0f, 1.0f);
}