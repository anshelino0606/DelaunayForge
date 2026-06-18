public struct VSDefaultInput {
    public float3 position : POSITION;
};

public struct VSDefaultOutput {
    public float4 pos : SV_Position;
    public float height : TEXCOORD0;
};