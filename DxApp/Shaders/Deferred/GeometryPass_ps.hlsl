#include "GeometryPass.hlsli"


struct OutputAttributes
{
	float4 albedoMetalness : SV_Target0;
	float4 positionRoughness : SV_Target1;
	float4 normalIor : SV_Target2;
};


// TODO cbuffer with roughness, metal, IOR
static const float kIor = 0.27f; //	gold
static const float kRoughness = 0.25f;
static const float kMetalness = 1.0f;


void ps_main(in PixelAttributes attributes, out OutputAttributes output)
{
	output.albedoMetalness = float4(attributes.color, kMetalness);
	output.positionRoughness = float4(attributes.worldPosition, kRoughness);
	output.normalIor = float4(attributes.normal, kIor);
}