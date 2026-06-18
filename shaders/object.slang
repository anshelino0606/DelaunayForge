import common.scene;
import common.vertex_layout_default;

struct Params {
    float u_min;
    float u_max;
    float2 padding;
};

ConstantBuffer<Scene> cScene;
ConstantBuffer<Params> cParams;

[shader("vertex")]
VSDefaultOutput vertexMain(VSDefaultInput input) {
    VSDefaultOutput output;
    output.height = input.position.y;
    output.pos = mul(cScene.projView, float4(input.position, 1.0f));
    return output;
}

[shader("pixel")]
float4 fragmentMain(VSDefaultOutput input) : SV_Target {
    float denom = max(0.0001, (cParams.u_max - cParams.u_min));
    float t = saturate((input.height - cParams.u_min) / denom);

    float3 color = float3(t, 50.0 / 255.0, 1.0 - t);

    return float4(color, 1);
}