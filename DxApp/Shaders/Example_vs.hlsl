#include "Example.hlsli"

struct VertexAttributes
{
    float3 position : POSITION;
    float4 color : COLOR;
};

void vs_main(in VertexAttributes input, out PixelAttributes output)
{
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
}