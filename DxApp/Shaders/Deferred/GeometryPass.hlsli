
#ifndef EXAMPLE_INCLUDES
#define EXAMPLE_INCLUDES

struct PixelAttributes
{
    float4 deviceCoordinatesPosition : SV_POSITION;
    float4 worldPosition : TEXCOORD0;
    float4 color : COLOR;
};

#endif