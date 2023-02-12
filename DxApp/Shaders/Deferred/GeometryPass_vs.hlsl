#include "GeometryPass.hlsli"


struct VertexAttributes
{
    float3 position : POSITION;
    float4 color : COLOR;
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
    output.worldPosition = mul(sceneData.model, float4(input.position, 1.0f));
    output.deviceCoordinatesPosition = mul(sceneData.vp, output.worldPosition);
    output.color = input.color;
}