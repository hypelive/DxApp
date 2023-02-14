#include "GeometryPass.hlsli"


struct OutputAttributes
{
	float4 albedo : SV_Target0;
	float4 position : SV_Target1;
	float4 normal : SV_Target2;
};

void ps_main(in PixelAttributes attributes, out OutputAttributes output)
{
	output.albedo = attributes.color;
	output.position = attributes.worldPosition;
	output.normal = attributes.normal;
}