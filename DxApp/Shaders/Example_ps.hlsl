struct PixelAttributes
{
    float4 position : SV_POSITION;
};

void ps_main(in PixelAttributes attributes, out float4 outputColor : SV_TARGET)
{
    outputColor = float4(1.0, 1.0, 0.0, 1.0);
}