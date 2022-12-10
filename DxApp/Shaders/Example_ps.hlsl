#include "Example.hlsli"

void ps_main(in PixelAttributes attributes, out float4 outputColor : SV_TARGET)
{
    outputColor = attributes.color;
}