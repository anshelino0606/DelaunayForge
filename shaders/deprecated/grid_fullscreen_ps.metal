#include <metal_stdlib>
using namespace metal;

struct GridVSParams
{
    float4x4 invViewProj;
    float4   cameraPos;
};

struct GridPSParams
{
    float4 colorMinor;
    float4 misc;
};

struct PSIn
{
    float4 pos [[position]];
    float2 ndc;
};

static float grid_line(float coord)
{
    float f = fract(coord);
    float dist = min(f, 1.0f - f);
    float w = fwidth(coord);

    return 1.0f - smoothstep(0.0f, w * 1.5f, dist);
}

fragment float4 fragment_main(PSIn input [[stage_in]],
                              constant GridVSParams& vsParams [[buffer(1)]],
                              constant GridPSParams& psParams [[buffer(2)]])
{
    const float cellSize = max(psParams.misc.x, 0.001f);
    const float renderDist = max(psParams.misc.y, cellSize);
    const float majorAlpha = clamp(psParams.misc.z, 0.0f, 1.0f);

    float4 pFar = vsParams.invViewProj * float4(input.ndc, 1.0f, 1.0f);
    pFar.xyz /= max(pFar.w, 1e-6f);

    float3 ro = vsParams.cameraPos.xyz;
    float3 rd = normalize(pFar.xyz - ro);

    if (fabs(rd.y) < 1e-6f) return float4(0,0,0,0);
    float t = (-ro.y) / rd.y;
    if (t <= 0.0f) return float4(0,0,0,0);

    float3 wp = ro + rd * t;
    float2 xz = float2(wp.x, wp.z);

    float dist = length(xz - float2(ro.x, ro.z));
    if (dist > renderDist) return float4(0,0,0,0);

    float2 c = xz / cellSize;
    float minor = max(grid_line(c.x), grid_line(c.y));

    float2 cm = xz / (cellSize * 5.0f);
    float major = max(grid_line(cm.x), grid_line(cm.y));

    float alpha = clamp(psParams.colorMinor.a, 0.0f, 1.0f) * minor;
    alpha = max(alpha, majorAlpha * major);

    float axisW = max(fwidth(wp.x), fwidth(wp.z)) * 2.0f;
    float axX = 1.0f - smoothstep(0.0f, axisW, fabs(wp.x));
    float axZ = 1.0f - smoothstep(0.0f, axisW, fabs(wp.z));

    float3 col = psParams.colorMinor.rgb;
    float3 axisColX = float3(1.0f, 0.35f, 0.35f);
    float3 axisColZ = float3(0.35f, 0.45f, 1.0f);

    col = mix(col, axisColX, clamp(axX, 0.0f, 1.0f));
    col = mix(col, axisColZ, clamp(axZ, 0.0f, 1.0f));
    alpha = max(alpha, 0.55f * max(axX, axZ));

    float fade = clamp(1.0f - dist / renderDist, 0.0f, 1.0f);
    alpha *= fade;

    return float4(col, alpha);
}
