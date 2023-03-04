#include "GeometryPass.hlsli"


struct VertexAttributes
{
    float4 position : POSITION;
    float4 color : COLOR;
    float4 normal : NORMAL;
};

struct SceneObjectData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;

    float4x4 mvp;

    float4x4 vp;
};

cbuffer ConstantBuffer : register(b0)
{
    SceneObjectData sceneData;
}

void vs_main(in VertexAttributes input, out PixelAttributes output)
{
    output.worldPosition = mul(sceneData.model, input.position).xyz;
    output.deviceCoordinatesPosition = mul(sceneData.vp, float4(output.worldPosition, 1.0f));
    output.color = input.color.xyz;
    output.normal = mul(sceneData.model, input.normal).xyz; // normal.w = 0 here, so translation ignored
}