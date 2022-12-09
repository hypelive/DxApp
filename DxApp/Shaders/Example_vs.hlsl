struct VertexAttributes
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PixelAttributes
{
    float4 position : SV_POSITION;
};

void vs_main(in VertexAttributes input, out PixelAttributes output)
{
    output.position = float4(input.position, 1.0f);
}