#include <metal_stdlib>
using namespace metal;

struct GridParams
{
    float4 color;
};

struct PS_INPUT
{
    float4 pos [[position]];
    float  height;
};

fragment float4 fragment_main(PS_INPUT input [[stage_in]],
                              constant GridParams& params [[buffer(2)]])
{
    (void)input;
    return params.color;
}
