
#ifndef GEOMETRY_PASS_INCLUDES
#define GEOMETRY_PASS_INCLUDES

struct PixelAttributes
{
    float4 deviceCoordinatesPosition : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 color : COLOR;
    float3 normal : NORMAL;
};

#endif