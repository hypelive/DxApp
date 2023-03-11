#include "GeometryPass.hlsli"


struct OutputAttributes
{
	float4 surfaceColor : SV_Target0;
	float4 positionRoughness : SV_Target1;
	float4 normalMetalness : SV_Target2;
	float4 fresnelIndices : SV_Target3;
};


// TODO cbuffer with roughness, metal, fresnelIndices
static const float3 kFresnelIndices = float3(0.6f, 0.7f, 0.8f);
static const float kRoughness = 0.1f;
static const float kMetalness = 0.0f;


void ps_main(in PixelAttributes attributes, out OutputAttributes output)
{
	output.surfaceColor = float4(attributes.color, 1.0f);
	output.positionRoughness = float4(attributes.worldPosition, kRoughness);
	output.normalMetalness = float4(attributes.normal, kMetalness);
	output.fresnelIndices = float4(kFresnelIndices, 1.0f);
}