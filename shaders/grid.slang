import common.scene;
import common.vertex_layout_default;

struct GridParams
{
    float4 color;
};

ConstantBuffer<Scene> cScene;
ConstantBuffer<GridParams> cParams;

[shader("vertex")]
VSDefaultOutput vertexMain(VSDefaultInput input) {
    VSDefaultOutput output;
    output.height = input.position.y;
    output.pos = mul(cScene.projView, float4(input.position, 1.0f));
    return output;
}

[shader("pixel")]
float4 fragmentMain(VSDefaultOutput input) : SV_Target
{
    return cParams.color;
}
