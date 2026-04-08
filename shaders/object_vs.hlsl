cbuffer SceneBuffer : register(b0) {
    float4x4 projView;
};

struct VS_INPUT {
    float3 position : POSITION;
};

struct VS_OUTPUT {
    float4 pos   : SV_Position;
    float height : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.height = input.position.y;
    output.pos   = mul(projView, float4(input.position, 1.0f));
    return output;
}