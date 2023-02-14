
#ifndef GEOMETRY_PASS_INCLUDES
#define GEOMETRY_PASS_INCLUDES

struct PixelAttributes
{
    float4 deviceCoordinatesPosition : SV_POSITION;
    float4 worldPosition : TEXCOORD0;
    float4 color : COLOR;
    float4 normal : NORMAL;
};

#endif