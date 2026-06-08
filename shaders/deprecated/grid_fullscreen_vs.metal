#include <metal_stdlib>
using namespace metal;

struct VSOut
{
    float4 pos [[position]];
    float2 ndc;
};

vertex VSOut vertex_main(uint vid [[vertex_id]])
{
    float2 p;
    if (vid == 0)      p = float2(-1.0, -1.0);
    else if (vid == 1) p = float2(-1.0,  3.0);
    else              p = float2( 3.0, -1.0);

    VSOut o;
    o.pos = float4(p, 0.0, 1.0);
    o.ndc = p;
    return o;
}
