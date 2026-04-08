cbuffer UIData : register(b0) {
    float2 scale;
    float2 translate;
};

struct VS_INPUT
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    output.color = input.color;
    output.uv = input.uv;

    float2 scaled = input.position * scale + translate;
    scaled.y *= -1.0f;
    output.position = float4(scaled, 0.0f, 1.0f);

    return output;
}

