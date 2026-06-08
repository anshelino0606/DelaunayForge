struct VS_INPUT {
    float3 position : POSITION;
};

struct VS_OUTPUT {
    float4 pos   : SV_Position;
    float height : TEXCOORD0;
};

struct SceneData {
    float4x4 projView;
};

struct Params {
    float u_min;
    float u_max;
    float2 padding;
};

ConstantBuffer<SceneData> cScene;
ConstantBuffer<Params> cParams;

[shader("vertex")]
VS_OUTPUT vertex_main(VS_INPUT input) {
    VS_OUTPUT output;
    output.height = input.position.y;
    output.pos = mul(cScene.projView, float4(input.position, 1.0f));
    return output;
}

[shader("pixel")]
float4 fragment_main(VS_OUTPUT input) : SV_Target {
    float denom = max(0.0001, (cParams.u_max - cParams.u_min));
    float t = saturate((input.height - cParams.u_min) / denom);

    float3 color = float3(t, 50.0 / 255.0, 1.0 - t);

    return float4(color, 1);
}