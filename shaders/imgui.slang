struct Params {
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

ConstantBuffer<Params> cParams;

[shader("vertex")]
VS_OUTPUT vertex_main(VS_INPUT input)
{
    VS_OUTPUT output;

    output.color = input.color;
    output.uv = input.uv;

    float2 scaled = input.position * cParams.scale + cParams.translate;
    scaled.y *= -1.0f;
    output.position = float4(scaled, 0.0f, 1.0f);

    return output;
}

Texture2D tex : register(t0);
SamplerState samplerLinear : register(s0);

[shader("pixel")]
float4 fragment_main(VS_OUTPUT input) : SV_Target {
    float4 textureColor = tex.Sample(samplerLinear, input.uv);
    return input.color * textureColor;
}
