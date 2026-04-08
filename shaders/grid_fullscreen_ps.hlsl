cbuffer GridVSParams : register(b0)
{
    float4x4 invViewProj;
    float4   cameraPos;
};

cbuffer GridPSParams : register(b1)
{
    float4 colorMinor;
    float4 misc;
};

struct PS_IN
{
    float4 pos : SV_Position;
    float2 ndc : TEXCOORD0;
};

static float grid_line(float coord)
{
    float f = frac(coord);
    float dist = min(f, 1.0 - f);
    float w = fwidth(coord);
    // w = saturate(w / 0.02) * 0.02;
    return 1.0 - smoothstep(0.0, w * 1.5, dist);
}

float4 main(PS_IN input) : SV_Target
{
    const float cellSize = max(misc.x, 0.001);
    const float renderDist = max(misc.y, cellSize);
    const float majorAlpha = saturate(misc.z);

    float4 pFar = mul(invViewProj, float4(input.ndc, 1.0, 1.0));
    pFar.xyz /= max(pFar.w, 1e-6);

    float3 ro = cameraPos.xyz;
    float3 rd = normalize(pFar.xyz - ro);

    if (abs(rd.y) < 1e-6) return float4(0,0,0,0);
    float t = (-ro.y) / rd.y;
    if (t <= 0.0) return float4(0,0,0,0);

    float3 wp = ro + rd * t;
    float2 xz = float2(wp.x, wp.z);

    float dist = length(xz - float2(ro.x, ro.z));
    if (dist > renderDist) return float4(0,0,0,0);

    float2 c = xz / cellSize;
    float minor = max(grid_line(c.x), grid_line(c.y));

    float2 cm = xz / (cellSize * 5.0);
    float major = max(grid_line(cm.x), grid_line(cm.y));

    float alpha = saturate(colorMinor.a) * minor;
    alpha = max(alpha, majorAlpha * major);

    float axisW = max(fwidth(wp.x), fwidth(wp.z)) * 2.0;
    float axX = 1.0 - smoothstep(0.0, axisW, abs(wp.x));
    float axZ = 1.0 - smoothstep(0.0, axisW, abs(wp.z));

    float3 col = colorMinor.rgb;
    float3 axisColX = float3(1.0, 0.35, 0.35);
    float3 axisColZ = float3(0.35, 0.45, 1.0);

    col = lerp(col, axisColX, saturate(axX));
    col = lerp(col, axisColZ, saturate(axZ));
    alpha = max(alpha, 0.55 * max(axX, axZ));

    float fade = saturate(1.0 - dist / renderDist);
    alpha *= fade;

    return float4(col, alpha);
}
