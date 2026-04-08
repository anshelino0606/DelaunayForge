// I think we want full bindless for all render passes, but for now only for ImGui renderer
Texture2D tex : register(t0);
SamplerState samplerLinear : register(s0);

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PS_INPUT input) : SV_Target {
    float4 textureColor = tex.Sample(samplerLinear, input.uv);
    return input.color * textureColor;
}
