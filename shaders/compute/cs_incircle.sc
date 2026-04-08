
#include <bgfx_shader.sh>
#include <bgfx_compute.sh>

BUFFER_RO(pointsBuffer,    vec4,  0);
BUFFER_RO(trianglesBuffer, uvec4, 1);
BUFFER_RO(testPointBuffer, vec4,  2);

IMAGE2D_RW(resultImage, r32ui, 0);

uniform vec4 u_params;

float orient2d(vec2 pa, vec2 pb, vec2 pc) {
    float acx = pa.x - pc.x;
    float bcx = pb.x - pc.x;
    float acy = pa.y - pc.y;
    float bcy = pb.y - pc.y;
    return acx * bcy - acy * bcx;
}

float inCircle(vec2 pa, vec2 pb, vec2 pc, vec2 pd) {
    float adx = pa.x - pd.x, ady = pa.y - pd.y;
    float bdx = pb.x - pd.x, bdy = pb.y - pd.y;
    float cdx = pc.x - pd.x, cdy = pc.y - pd.y;

    float abdet = adx * bdy - bdx * ady;
    float bcdet = bdx * cdy - cdx * bdy;
    float cadet = cdx * ady - adx * cdy;

    float alift = adx * adx + ady * ady;
    float blift = bdx * bdx + bdy * bdy;
    float clift = cdx * cdx + cdy * cdy;

    return alift * bcdet + blift * cadet + clift * abdet;
}

NUM_THREADS(64, 1, 1)
void main()
{
    uint tid = gl_GlobalInvocationID.x;
    uint numTris = uint(u_params.x);
    if (tid >= numTris) return;

    uvec4 tri = trianglesBuffer[tid];
    uint outVal = 0u;

    if (tri.w != 0u) {
        vec2 pa = pointsBuffer[tri.x].xy;
        vec2 pb = pointsBuffer[tri.y].xy;
        vec2 pc = pointsBuffer[tri.z].xy;
        vec2 pd = testPointBuffer[0].xy;

        // enforce CCW
        if (orient2d(pa, pb, pc) < 0.0) {
            vec2 tmp = pb; pb = pc; pc = tmp;
        }

        float eps = u_params.y;
        float s = inCircle(pa, pb, pc, pd);
        outVal = (s > eps) ? 1u : 0u;
    }

#if BGFX_SHADER_LANGUAGE_HLSL
    resultImage[int2(int(tid), 0)] = outVal;
#else
    imageStore(resultImage, ivec2(int(tid), 0), uvec4(outVal,0u,0u,0u));
#endif
}