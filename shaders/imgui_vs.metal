#include <metal_stdlib>
using namespace metal;

// Structure for our Constant Buffer (b0)
struct UIData {
    float2 scale;
    float2 translate;
};

// Input attributes (mapped via vertex descriptor)
struct VS_INPUT {
    float2 position [[attribute(0)]];
    float2 uv       [[attribute(1)]];
    uchar4 color    [[attribute(2)]];
};

// Output to Rasterizer
struct VS_OUTPUT {
    float4 position [[position]];
    float2 uv;
    float4 color;
};

vertex VS_OUTPUT vertex_main(VS_INPUT input [[stage_in]],
                             constant UIData& uiData [[buffer(1)]])
{
    VS_OUTPUT output;

    output.color = float4(input.color) / 255.0f;
    output.uv = input.uv;

    // Apply scale and translation
    float2 scaled = input.position * uiData.scale + uiData.translate;

    // Metal NDC has Y up; ImGui provides Y down.
    scaled.y *= -1.0f;

    output.position = float4(scaled, 0.0f, 1.0f);
    return output;
}