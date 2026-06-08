struct VS_INPUT {
    float3 position : POSITION;
};

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float height : TEXCOORD0;
};

struct SceneData {
    float4x4 projView;
};

struct GridParams
{
    float4 color;
};

ConstantBuffer<SceneData> cScene;
ConstantBuffer<GridParams> cParams;

[shader("vertex")]
VS_OUTPUT vertex_main(VS_INPUT input) {
    VS_OUTPUT output;
    output.height = input.position.y;
    output.pos = mul(cScene.projView, float4(input.position, 1.0f));
    return output;
}

[shader("pixel")]
float4 fragment_main(VS_OUTPUT input) : SV_Target
{
    return cParams.color;
}
