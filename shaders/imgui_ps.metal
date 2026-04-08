#include <metal_stdlib>
using namespace metal;

struct PS_INPUT {
    float4 position [[position]];
    float2 uv;
    float4 color;
};

fragment float4 fragment_main(PS_INPUT input [[stage_in]],
                               texture2d<float> tex [[texture(0)]],
                               sampler samplerLinear [[sampler(0)]])
{
    float4 texCol = tex.sample(samplerLinear, input.uv);
    return texCol * input.color;
}