cbuffer GridParams : register(b1)
{
    float4 color;
};

struct PS_INPUT
{
    float4 pos   : SV_Position;
    float height : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target
{
    return color;
}
