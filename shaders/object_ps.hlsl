cbuffer Params : register(b1) {
    float u_min;
    float u_max;
    float2 padding;
};

struct PS_INPUT {
    float4 pos   : SV_Position;
    float height : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target {
    float denom = max(0.0001, (u_max - u_min));
    float t = saturate((input.height - u_min) / denom);

    float3 color = float3(t, 50.0 / 255.0, 1.0 - t);

    return float4(color, 1);
}