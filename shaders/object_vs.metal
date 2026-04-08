#include <metal_stdlib>
using namespace metal;

struct SceneBuffer {
    float4x4 projView;
};

struct VS_INPUT {
    float3 position [[attribute(0)]];
};

struct VS_OUTPUT {
    float4 pos [[position]];
    float  height;
};

vertex VS_OUTPUT vertex_main(VS_INPUT input [[stage_in]],
                             constant SceneBuffer& scene [[buffer(1)]])
{
    VS_OUTPUT output;

    output.pos = scene.projView * float4(input.position, 1.0f);
    
    output.height = input.position.y;

    return output;
}