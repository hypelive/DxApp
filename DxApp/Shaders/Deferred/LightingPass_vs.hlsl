#include "LightingPass.hlsli"


struct Vertex
{
    float4 position;
    float2 uv;
};


void vs_main(in uint vertexId : SV_VertexId, out PixelAttributes output)
{
    const Vertex kQuadVertices[] = {
    	{float4(-1.0f, -1.0f, 0.0f, 1.0f), float2(0.0f, 1.0f)},
        {float4(1.0f, 1.0f, 0.0f, 1.0f), float2(1.0f, 0.0f)},
        {float4(-1.0f, 1.0f, 0.0f, 1.0f), float2(0.0f, 0.0f)},
        {float4(-1.0f, -1.0f, 0.0f, 1.0f), float2(0.0f, 1.0f)},
        {float4(1.0f, -1.0f, 0.0f, 1.0f), float2(1.0f, 1.0f)},
        {float4(-1.0f, -1.0f, 0.0f, 1.0f), float2(1.0f, 0.0f)}
    };

    Vertex vertexData = kQuadVertices[vertexId];
    output.position = vertexData.position;
    output.uv = vertexData.uv;
}