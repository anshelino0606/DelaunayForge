#include <metal_stdlib>
using namespace metal;

struct Params {
    float u_min;
    float u_max;
    float2 padding;
};

struct PS_INPUT {
    float4 pos [[position]];
    float  height;
};

fragment float4 fragment_main(PS_INPUT input [[stage_in]],
                               constant Params& params [[buffer(2)]])
{
    float t = saturate((input.height - params.u_min) / (max(0.0001f, params.u_max - params.u_min)));
    
    float3 blue = float3(0.0, 0.0, 1.0);
    float3 red  = float3(1.0, 0.0, 0.0);
    float3 color = mix(blue, red, t);

    return float4(color, 1.0);
}