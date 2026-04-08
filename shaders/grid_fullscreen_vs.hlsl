struct VS_OUT
{
    float4 pos : SV_Position;
    float2 ndc : TEXCOORD0;
};

VS_OUT main(uint vid : SV_VertexID)
{
    float2 p;
    if (vid == 0) p = float2(-1.0, -1.0);
    else if (vid == 1) p = float2(-1.0,  3.0);
    else p = float2( 3.0, -1.0);

    VS_OUT o;
    o.pos = float4(p, 0.0, 1.0);
    o.ndc = p;
    return o;
}
